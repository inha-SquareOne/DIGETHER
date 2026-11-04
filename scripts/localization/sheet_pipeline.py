#!/usr/bin/env python3
"""Convert an exported spreadsheet CSV into reviewable UE localization inputs.

Outputs are always new files. The caller is responsible for importing them into
Unreal and compiling localization resources; this script never edits assets.
"""
from __future__ import annotations

import argparse
import codecs
import csv
import io
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


class InputError(ValueError):
    pass


def read_text(path: Path) -> str:
    data = path.read_bytes()
    if data.startswith((codecs.BOM_UTF32_LE, codecs.BOM_UTF32_BE)):
        raise InputError(f"{path}: UTF-32 is not supported")
    encoding = "utf-16" if data.startswith((codecs.BOM_UTF16_LE, codecs.BOM_UTF16_BE)) else "utf-8-sig"
    return data.decode(encoding)


def read_csv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with io.StringIO(read_text(path), newline="") as handle:
        reader = csv.DictReader(handle, strict=True)
        fields = reader.fieldnames
        if not fields or any(not field for field in fields) or len(set(fields)) != len(fields):
            raise InputError(f"{path}: missing or duplicate CSV headers")
        rows = []
        for number, row in enumerate(reader, 2):
            if None in row or any(value is None for value in row.values()):
                raise InputError(f"{path}:{number}: wrong column count")
            if any(row.values()):
                rows.append(row)
        return fields, rows


def sheet_rows(path: Path, table: str) -> list[dict[str, str]]:
    fields, rows = read_csv(path)
    required = {"Table", "Key", "SourceString", "POContext"}
    if not required.issubset(fields):
        raise InputError(f"sheet requires columns: {', '.join(sorted(required))}")
    selected = [row for row in rows if row["Table"] == table]
    if not selected:
        raise InputError(f"no rows for table {table!r}")
    keys: set[str] = set()
    for row in selected:
        key = row["Key"]
        if not key or key != key.strip() or not row["SourceString"]:
            raise InputError("selected rows must have a nonempty, unpadded Key and SourceString")
        if key in keys:
            raise InputError(f"duplicate sheet key: {key}")
        keys.add(key)
    return selected


_UE_ESCAPES = [("\\", "\\\\"), ("\n", "\\n"), ("\r", "\\r"),
               ("\t", "\\t"), ("'", "\\'"), ('"', '\\"')]


def _ue_unescape_actual(value: str) -> str:
    # FString::ReplaceEscapedCharWithChar applies replacements in reverse order.
    for char, escaped in reversed(_UE_ESCAPES):
        value = value.replace(escaped, char)
    return value


def ue_escape(value: str) -> str:
    original = value
    for char, escaped in _UE_ESCAPES:
        value = value.replace(char, escaped)
    if _ue_unescape_actual(value) != original:
        raise InputError("text cannot round-trip through Unreal CSV escaping")
    return value


def ue_unescape(value: str) -> str:
    decoded = _ue_unescape_actual(value)
    if ue_escape(decoded) != value:
        raise InputError("baseline contains an ambiguous UE escape sequence")
    return decoded


def baseline_rows(path: Path) -> tuple[list[str], dict[str, dict[str, str]]]:
    fields, rows = read_csv(path)
    if not {"Key", "SourceString"}.issubset(fields):
        raise InputError("baseline requires Key and SourceString headers")
    old: dict[str, dict[str, str]] = {}
    for row in rows:
        key = ue_unescape(row["Key"])
        if not key or key in old:
            raise InputError(f"baseline has empty or duplicate key: {key!r}")
        # UE exports metadata without escape conversion but unescapes it on import.
        # Refuse ambiguous backslashes instead of silently changing metadata.
        for field in fields:
            if field not in {"Key", "SourceString", "DevNotes"} and "\\" in row[field]:
                raise InputError(f"baseline metadata {field!r} contains an ambiguous backslash")
        old[key] = row
    if not old:
        raise InputError("baseline is empty; export the existing table first")
    return fields, old


def bootstrap_csv(baseline: Path, table: str, cultures: list[str]) -> str:
    _, baseline_entries = baseline_rows(baseline)
    if not table or not cultures or len(set(cultures)) != len(cultures):
        raise InputError("provide a table and at least one distinct culture")
    if any(not re.fullmatch(r"[A-Za-z0-9-]+", value) for value in cultures):
        raise InputError("culture names must use letters, digits and hyphens")
    fields = ["Table", "Key", "SourceString", "POContext"]
    for culture in cultures:
        fields.extend((f"Translation:{culture}", f"ReviewedSource:{culture}"))
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fields, lineterminator="\n")
    writer.writeheader()
    for key, old in baseline_entries.items():
        writer.writerow({"Table": table, "Key": key, "SourceString": ue_unescape(old["SourceString"]), "POContext": ""})
    return output.getvalue()


def table_csv(sheet: Path, baseline: Path, current: Path, table: str) -> str:
    if baseline.samefile(current):
        raise InputError("current export must be a separate fresh file, not the baseline")
    rows = sheet_rows(sheet, table)
    fields, old = baseline_rows(baseline)
    current_fields, current_rows = baseline_rows(current)
    if set(fields) != set(current_fields):
        raise InputError("current table export has different columns from baseline; refresh and review")
    if old != current_rows:
        raise InputError("current table export differs from baseline; refresh and review")
    missing = old.keys() - {row["Key"] for row in rows}
    if missing:
        raise InputError(f"sheet omits baseline keys: {', '.join(sorted(missing))}")
    result = io.StringIO(newline="")
    writer = csv.DictWriter(result, fields, lineterminator="\n")
    writer.writeheader()
    for row in rows:
        item = dict(old.get(row["Key"], {field: "" for field in fields}))
        item["Key"] = ue_escape(row["Key"])
        item["SourceString"] = ue_escape(row["SourceString"])
        writer.writerow(item)
    return result.getvalue()


@dataclass
class POEntry:
    context: str | None
    source: str
    translation: str
    start: int
    end: int
    plural: bool
    fuzzy: bool


_DIRECTIVE = re.compile(r'^(msgctxt|msgid|msgid_plural|msgstr(?:\[\d+\])?)\s+(".*")\s*$')


def po_string(text: str) -> str:
    try:
        value = json.loads(text)
    except (ValueError, TypeError) as exc:
        raise InputError(f"unsupported PO quoted string: {text!r}") from exc
    if not isinstance(value, str):
        raise InputError("PO value must be a string")
    return value


def parse_po(text: str) -> tuple[list[str], list[POEntry]]:
    lines = text.splitlines(keepends=True)
    entries: list[POEntry] = []
    block: list[int] = []

    def finish() -> None:
        if not block:
            return
        fields: dict[str, str] = {}
        spans: dict[str, list[int]] = {}
        active: str | None = None
        fuzzy = False
        for index in block:
            line = lines[index].rstrip("\r\n")
            if line.startswith("#"):
                if line.startswith("#,") and "fuzzy" in [v.strip() for v in line[2:].split(",")]:
                    fuzzy = True
                active = None
                continue
            match = _DIRECTIVE.match(line)
            if match:
                name, value = match.groups()
                if name in fields:
                    raise InputError(f"duplicate PO directive {name}")
                fields[name] = po_string(value)
                spans[name] = [index, index + 1]
                active = name
            elif line.startswith('"') and active is not None:
                fields[active] += po_string(line)
                spans[active][1] = index + 1
            else:
                raise InputError(f"unsupported PO syntax on line {index + 1}")
        if "msgid" not in fields:
            # Standalone trailing comments are preserved unchanged.
            if fields:
                raise InputError("PO block missing msgid")
            return
        plural = "msgid_plural" in fields or any(k.startswith("msgstr[") for k in fields)
        if not plural and "msgstr" not in fields:
            raise InputError("PO block missing msgstr")
        span = spans.get("msgstr", [0, 0])
        entries.append(POEntry(fields.get("msgctxt"), fields["msgid"], fields.get("msgstr", ""), span[0], span[1], plural, fuzzy))

    for index, line in enumerate(lines):
        if line.strip():
            block.append(index)
        else:
            finish()
            block = []
    finish()
    return lines, entries


def placeholders(value: str) -> set[str]:
    names: set[str] = set()
    index = 0
    while index < len(value):
        if value[index] == "`" and index + 1 < len(value):
            index += 2
            continue
        if value[index] == "{":
            end = value.find("}", index + 1)
            name = value[index + 1:end] if end >= 0 else ""
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*|[0-9]+", name):
                raise InputError("unsupported or malformed FText placeholder")
            names.add(name)
            index = end + 1
            continue
        if value[index] == "}":
            raise InputError("unmatched closing brace in FText")
        index += 1
    return names


def parse_ue_context(context: str) -> tuple[str, str]:
    # PortableObjectPipeline::ParseIdentityFromPO uses the first unescaped comma.
    escaped = False
    for index, char in enumerate(context):
        if escaped:
            escaped = False
        elif char == "\\":
            escaped = True
        elif char == ",":
            return (context[:index].replace("\\,", ","),
                    context[index + 1:].replace("\\,", ","))
    raise InputError("POContext is not an Unreal namespace,key identity")


def translated_po(sheet: Path, po: Path, table: str, culture: str, namespace: str) -> str:
    fields, _ = read_csv(sheet)
    column = f"Translation:{culture}"
    reviewed_column = f"ReviewedSource:{culture}"
    for field in (column, reviewed_column):
        if field not in fields:
            raise InputError(f"sheet missing column {field!r}")
    rows = sheet_rows(sheet, table)
    # splitlines(keepends=True) preserves the exported PO's original line endings.
    lines, entries = parse_po(read_text(po))
    headers = [entry for entry in entries if entry.context is None and entry.source == ""]
    if len(headers) != 1:
        raise InputError("PO must contain exactly one header")
    languages = [line.partition(":")[2].strip() for line in headers[0].translation.splitlines()
                 if line.startswith("Language:")]
    if languages != [culture]:
        raise InputError(f"PO Language header does not match {culture!r}")
    by_context: dict[str, list[POEntry]] = {}
    for entry in entries:
        if entry.context is not None:
            by_context.setdefault(entry.context, []).append(entry)
    used: set[str] = set()
    replacements: list[tuple[int, int, str]] = []
    for row in rows:
        context, source, translated = row["POContext"], row["SourceString"], row[column]
        if not context or context in used:
            raise InputError(f"missing or duplicate POContext for {row['Key']}")
        used.add(context)
        actual_namespace, actual_key = parse_ue_context(context)
        if (actual_namespace, actual_key) != (namespace, row["Key"]):
            raise InputError(f"POContext namespace or key does not match {row['Key']}")
        matches = [entry for entry in by_context.get(context, []) if entry.source == source]
        if len(matches) != 1:
            raise InputError(f"missing, stale or ambiguous PO identity for {row['Key']}")
        entry = matches[0]
        if entry.plural or entry.fuzzy:
            raise InputError(f"plural or fuzzy entry requires manual review: {row['Key']}")
        if not translated:
            raise InputError(f"empty translation: {row['Key']}")
        if row[reviewed_column] != source:
            raise InputError(f"translation not reviewed against current source: {row['Key']}")
        if placeholders(source) != placeholders(translated):
            raise InputError(f"FText placeholders differ: {row['Key']}")
        ending = "\r\n" if lines[entry.start].endswith("\r\n") else "\n"
        replacement = "msgstr " + json.dumps(translated, ensure_ascii=False) + ending
        replacements.append((entry.start, entry.end, replacement))
    for start, end, replacement in sorted(replacements, reverse=True):
        lines[start:end] = [replacement]
    return "".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    bootstrap = sub.add_parser("bootstrap", help="make a Sheets starter CSV from an existing UE table export")
    bootstrap.add_argument("--baseline", type=Path, required=True)
    bootstrap.add_argument("--table", required=True)
    bootstrap.add_argument("--culture", action="append", required=True)
    bootstrap.add_argument("--output", type=Path, required=True)
    table = sub.add_parser("table", help="prepare a complete UE String Table CSV from a baseline")
    table.add_argument("--sheet", type=Path, required=True)
    table.add_argument("--baseline", type=Path, required=True)
    table.add_argument("--current", type=Path, required=True,
                       help="fresh export of the same saved asset taken immediately before import")
    table.add_argument("--table", required=True)
    table.add_argument("--output", type=Path, required=True)
    po = sub.add_parser("po", help="fill an Unreal-exported PO with translations")
    po.add_argument("--sheet", type=Path, required=True)
    po.add_argument("--input-po", type=Path, required=True)
    po.add_argument("--table", required=True)
    po.add_argument("--culture", required=True)
    po.add_argument("--namespace", required=True, help="exact namespace of the String Table asset")
    po.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "bootstrap":
            output = bootstrap_csv(args.baseline, args.table, args.culture)
        elif args.command == "table":
            output = table_csv(args.sheet, args.baseline, args.current, args.table)
        else:
            output = translated_po(args.sheet, args.input_po, args.table, args.culture, args.namespace)
        # Exclusive create makes accidental replacement of the baseline impossible.
        with args.output.open("x", encoding="utf-8", newline="") as handle:
            handle.write(output)
    except (InputError, OSError, csv.Error, UnicodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(f"Created {args.output}; review before importing into Unreal.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
