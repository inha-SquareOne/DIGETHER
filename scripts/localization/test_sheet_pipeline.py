import codecs
import csv
import io
import tempfile
import unittest
from pathlib import Path

import sheet_pipeline as pipeline


class SheetPipelineTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.sheet = self.root / "sheet.csv"
        self.baseline = self.root / "baseline.csv"
        self.current = self.root / "current.csv"
        self.po = self.root / "in.po"
        self.sheet_rows = [
            {"Table": "Session", "Key": "Greeting", "SourceString": "Hello {Name}",
             "POContext": "ns,Greeting", "Translation:xx": "안녕 {Name}",
             "ReviewedSource:xx": "Hello {Name}"},
            {"Table": "Session", "Key": "Path", "SourceString": "a\\b\nnext",
             "POContext": "ns,Path", "Translation:xx": "경로\n다음",
             "ReviewedSource:xx": "a\\b\nnext"},
        ]
        self.write_sheet()
        self.baseline.write_text('Key,SourceString,Comment\nGreeting,Old,keep\nPath,Old,kept\n', encoding="utf-8")
        self.current.write_bytes(self.baseline.read_bytes())
        self.po.write_text(
            '# header\nmsgid ""\nmsgstr ""\n"Content-Type: text/plain; charset=UTF-8\\n"\n"Language: xx\\n"\n\n'
            '#. translator context\nmsgctxt "ns,Greeting"\nmsgid "Hello {Name}"\nmsgstr "old"\n\n'
            'msgctxt "ns,Path"\nmsgid "a\\\\b\\nnext"\nmsgstr ""\n\n'
            '# untouched\nmsgctxt "other"\nmsgid "other"\nmsgstr "preserved"\n', encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def write_sheet(self):
        with self.sheet.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, ["Table", "Key", "SourceString", "POContext", "Translation:xx", "ReviewedSource:xx"])
            writer.writeheader()
            writer.writerows(self.sheet_rows)

    def test_table_preserves_baseline_keys_and_metadata_and_escapes(self):
        output = pipeline.table_csv(self.sheet, self.baseline, self.current, "Session")
        rows = list(csv.DictReader(io.StringIO(output)))
        self.assertEqual(rows[0]["Comment"], "keep")
        self.assertEqual(rows[1]["SourceString"], r"a\\b\nnext")

    def test_table_rejects_missing_baseline_key(self):
        self.sheet_rows.pop()
        self.write_sheet()
        with self.assertRaisesRegex(pipeline.InputError, "omits baseline keys: Path"):
            pipeline.table_csv(self.sheet, self.baseline, self.current, "Session")

    def test_duplicate_key_is_rejected(self):
        self.sheet_rows.append(dict(self.sheet_rows[0]))
        self.write_sheet()
        with self.assertRaisesRegex(pipeline.InputError, "duplicate sheet key"):
            pipeline.table_csv(self.sheet, self.baseline, self.current, "Session")

    def test_ambiguous_metadata_is_not_corrupted(self):
        self.baseline.write_text('Key,SourceString,Comment\nGreeting,Old,path\\name\n', encoding="utf-8")
        with self.assertRaisesRegex(pipeline.InputError, "ambiguous backslash"):
            pipeline.table_csv(self.sheet, self.baseline, self.current, "Session")

    def test_current_export_detects_changed_removed_added_and_metadata(self):
        fixtures = {
            "source changed": 'Key,SourceString,Comment\nGreeting,New,keep\nPath,Old,kept\n',
            "key removed": 'Key,SourceString,Comment\nGreeting,Old,keep\n',
            "key added": 'Key,SourceString,Comment\nGreeting,Old,keep\nPath,Old,kept\nExtra,New,note\n',
            "metadata changed": 'Key,SourceString,Comment\nGreeting,Old,edited\nPath,Old,kept\n',
        }
        for label, current in fixtures.items():
            with self.subTest(case=label):
                self.current.write_text(current, encoding="utf-8")
                with self.assertRaisesRegex(pipeline.InputError, "current table export differs"):
                    pipeline.table_csv(self.sheet, self.baseline, self.current, "Session")

    def test_current_export_rejects_different_metadata_columns(self):
        self.current.write_text('Key,SourceString,Note\nGreeting,Old,keep\nPath,Old,kept\n', encoding="utf-8")
        with self.assertRaisesRegex(pipeline.InputError, "different columns"):
            pipeline.table_csv(self.sheet, self.baseline, self.current, "Session")

    def test_current_export_must_not_be_baseline_file(self):
        with self.assertRaisesRegex(pipeline.InputError, "separate fresh file"):
            pipeline.table_csv(self.sheet, self.baseline, self.baseline, "Session")

    def test_current_export_compares_content_not_encoding_or_row_order(self):
        text = 'Comment,SourceString,Key\r\nkept,Old,Path\r\nkeep,Old,Greeting\r\n'
        self.current.write_bytes(codecs.BOM_UTF16_LE + text.encode("utf-16-le"))
        output = pipeline.table_csv(self.sheet, self.baseline, self.current, "Session")
        self.assertEqual(len(list(csv.DictReader(io.StringIO(output)))), 2)

    def test_current_mismatch_does_not_create_output(self):
        self.current.write_text('Key,SourceString,Comment\nGreeting,New,keep\nPath,Old,kept\n', encoding="utf-8")
        output = self.root / "not-created.csv"
        args = ["table", "--sheet", str(self.sheet), "--baseline", str(self.baseline),
                "--current", str(self.current), "--table", "Session", "--output", str(output)]
        self.assertEqual(pipeline.main(args), 1)
        self.assertFalse(output.exists())

    def test_po_matches_identity_preserving_other_entries_and_comments(self):
        output = pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns").replace("\r\n", "\n")
        self.assertIn('#. translator context\nmsgctxt "ns,Greeting"\nmsgid "Hello {Name}"\nmsgstr "안녕 {Name}"', output)
        self.assertIn('msgstr "경로\\n다음"', output)
        self.assertIn('# untouched\nmsgctxt "other"\nmsgid "other"\nmsgstr "preserved"', output)
        self.assertIn('"Content-Type: text/plain; charset=UTF-8\\n"', output)

    def test_bootstrap_decodes_ue_source_and_leaves_identity_blank(self):
        self.baseline.write_text('Key,SourceString\nGreeting,"Hi\\nfriend"\n', encoding="utf-8")
        output = pipeline.bootstrap_csv(self.baseline, "Session", ["xx", "yy"])
        rows = list(csv.DictReader(io.StringIO(output)))
        self.assertEqual(rows[0]["SourceString"], "Hi\nfriend")
        self.assertEqual(rows[0]["POContext"], "")
        self.assertEqual(rows[0]["Translation:xx"], "")
        self.assertEqual(rows[0]["Translation:yy"], "")
        self.assertEqual(rows[0]["ReviewedSource:xx"], "")
        self.assertEqual(rows[0]["ReviewedSource:yy"], "")

    def test_csv_import_supports_utf16_le_be_and_utf8_bom(self):
        value = 'Key,SourceString\r\nGreeting,안녕하세요\r\n'
        for encoding, bom in [("utf-16-le", codecs.BOM_UTF16_LE),
                              ("utf-16-be", codecs.BOM_UTF16_BE),
                              ("utf-8", codecs.BOM_UTF8)]:
            with self.subTest(encoding=encoding):
                self.baseline.write_bytes(bom + value.encode(encoding))
                output = pipeline.bootstrap_csv(self.baseline, "Session", ["xx"])
                self.assertEqual(next(csv.DictReader(io.StringIO(output)))["SourceString"], "안녕하세요")

    def test_utf32_bom_is_not_misread_as_utf16(self):
        self.baseline.write_bytes(codecs.BOM_UTF32_LE + 'Key,SourceString'.encode('utf-32-le'))
        with self.assertRaisesRegex(pipeline.InputError, "UTF-32 is not supported"):
            pipeline.bootstrap_csv(self.baseline, "Session", ["xx"])

    def test_stale_or_missing_source_is_rejected(self):
        self.sheet_rows[0]["SourceString"] = "Changed {Name}"
        self.write_sheet()
        with self.assertRaisesRegex(pipeline.InputError, "stale"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")

    def test_wrong_culture_header_is_rejected(self):
        po = self.po.read_text(encoding="utf-8").replace("Language: xx", "Language: yy")
        self.po.write_text(po, encoding="utf-8")
        with self.assertRaisesRegex(pipeline.InputError, "Language header"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")

    def test_stale_review_is_rejected_even_when_po_matches_new_source(self):
        self.sheet_rows[0]["SourceString"] = "Welcome {Name}"
        self.write_sheet()
        raw = self.po.read_text(encoding="utf-8").replace("Hello {Name}", "Welcome {Name}")
        self.po.write_text(raw, encoding="utf-8")
        with self.assertRaisesRegex(pipeline.InputError, "not reviewed against current source"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")

    def test_context_key_and_namespace_must_match(self):
        self.sheet_rows[0]["POContext"] = "ns,Path"
        self.write_sheet()
        with self.assertRaisesRegex(pipeline.InputError, "namespace or key"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")
        self.sheet_rows[0]["POContext"] = "other,Greeting"
        self.write_sheet()
        with self.assertRaisesRegex(pipeline.InputError, "namespace or key"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")

    def test_ue_context_escaped_comma(self):
        self.assertEqual(pipeline.parse_ue_context(r"space\,name,key\,name"), ("space,name", "key,name"))

    def test_unreal_ambiguous_backslash_roundtrip_is_rejected(self):
        with self.assertRaisesRegex(pipeline.InputError, "round-trip"):
            pipeline.ue_escape(r"literal\n")

    def test_empty_translation_and_missing_placeholder_rejected(self):
        self.sheet_rows[0]["Translation:xx"] = ""
        self.write_sheet()
        with self.assertRaisesRegex(pipeline.InputError, "empty translation"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")
        self.sheet_rows[0]["Translation:xx"] = "안녕하세요"
        self.write_sheet()
        with self.assertRaisesRegex(pipeline.InputError, "placeholders differ"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")

    def test_fuzzy_and_plural_are_not_rewritten(self):
        raw = self.po.read_text(encoding="utf-8")
        self.po.write_text(raw.replace('#. translator context', '#, fuzzy\n#. translator context'), encoding="utf-8")
        with self.assertRaisesRegex(pipeline.InputError, "fuzzy"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")
        self.po.write_text(raw.replace('msgstr "old"', 'msgid_plural "Hello all"\nmsgstr[0] "old"\nmsgstr[1] "old"'), encoding="utf-8")
        with self.assertRaisesRegex(pipeline.InputError, "plural"):
            pipeline.translated_po(self.sheet, self.po, "Session", "xx", "ns")

    def test_no_overwrite_and_no_file_after_failed_validation(self):
        existing = self.root / "existing.csv"
        existing.write_text("original", encoding="utf-8")
        args = ["table", "--sheet", str(self.sheet), "--baseline", str(self.baseline),
                "--current", str(self.current), "--table", "Session", "--output", str(existing)]
        self.assertEqual(pipeline.main(args), 1)
        self.assertEqual(existing.read_text(encoding="utf-8"), "original")
        self.sheet_rows.pop()
        self.write_sheet()
        missing = self.root / "never-created.csv"
        args[-1] = str(missing)
        self.assertEqual(pipeline.main(args), 1)
        self.assertFalse(missing.exists())


if __name__ == "__main__":
    unittest.main()
