"""격리 붓기 실행의 원본 로그와 슬롯 계획을 판정한다. Unreal은 실행하지 않는다."""
from __future__ import annotations

import argparse
import datetime as dt
import json
import math
from pathlib import Path
import re
import sys
import tempfile

STAMP = re.compile(r"\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]")
SLOT = re.compile(r"AutoCmd 실행: exec .*[/\\]slot-(\d+)\.txt", re.I)
RESPONSE = re.compile(r"납부 결과: Operation=\w+::(\w+) Result=\w+::(\w+).*?RateLimited=(\d+).*?Accepted=(\d+) Credited=(\d+)")


def utc(text):
    return dt.datetime.strptime(text, "%Y.%m.%d-%H.%M.%S:%f").replace(tzinfo=dt.timezone.utc).timestamp()


def fields(text):
    result, start, depth, quoted = [], 0, 0, False
    for index, char in enumerate(text):
        if char == '"' and (index == 0 or text[index - 1] != "\\"):
            quoted = not quoted
        if not quoted:
            depth += char == "("
            depth -= char == ")"
            if char == "," and depth == 0:
                result.append(text[start:index])
                start = index + 1
    result.append(text[start:])
    return result


def unpack(text):
    text = text.strip()
    if text.startswith("(") and text.endswith(")"):
        entries = fields(text[1:-1]) if text != "()" else []
        return {key: unpack(value) for key, value in (entry.split("=", 1) for entry in entries if "=" in entry)}
    if re.fullmatch(r"-?\d+", text):
        return int(text)
    return text.strip('"')


def vector(text):
    values = re.findall(r"(?:X|Y|Z|P|R)=(-?\d+(?:\.\d+)?)", text)
    return [float(value) for value in values] if len(values) == 3 else None


def parse_log(path, actions):
    text = Path(path).read_text(encoding="utf-8-sig", errors="replace") if Path(path).exists() else ""
    result = {"ready": False, "slots": [], "responses": [], "snapshots": [], "views": [],
              "teleports": [], "errors": [], "commands_failed": [], "exec_files": [],
              "lag": [], "loss": [], "lag_changes": [], "loss_changes": [], "quota_reset": [],
              "grants": [], "startup_errors": [], "server_sessions": []}
    stage, snapshot = None, None
    for line in text.splitlines():
        stamp = STAMP.search(line)
        time = utc(stamp[1]) if stamp else None
        match = SLOT.search(line)
        if match and time is not None:
            number = int(match[1])
            result["slots"].append({"slot": number, "utc": time})
            if number in actions:
                stage = actions[number]
        if "DigetherAutoCmds 예약:" in line:
            result["ready"] = True
        if "Execing " in line:
            result["exec_files"].append(line.split("Execing ", 1)[1])
        if "Command not recognized:" in line or "Can't find file" in line:
            result["commands_failed"].append(line)
        if re.search(r"LogDigether[^\r\n]*Error:|Fatal error:|Assertion failed:|EXCEPTION_ACCESS_VIOLATION", line):
            result["errors"].append(line)
        if "AttributeError:" in line:
            result["startup_errors"].append(line)
        for label, key in [("PktLag", "lag"), ("PktLoss", "loss")]:
            match = re.search(label + r" set to (\d+)", line)
            if match:
                result[key].append(int(match[1]))
                result[key + "_changes"].append({"utc": time, "value": int(match[1])})
        if "DigetherSetQuotaProgress 서버 결과:" in line:
            result["quota_reset"].append({"stage": stage, "utc": time, "success": "Applied=1" in line})
        if "DigetherGrantCredits 서버 결과:" in line:
            match = re.search(r"Amount=(\d+) Granted=(\d)", line)
            if match:
                result["grants"].append({"stage": stage, "amount": int(match[1]), "success": match[2] == "1"})
        if "DigetherTeleportToDeposit 서버 결과:" in line:
            result["teleports"].append({"stage": stage, "utc": time, "success": "Moved=1" in line, "line": line})
        match = re.search(r"Pour session: Event=(Begin|End) Player=(-?\d+) Active=(\d+)", line)
        if match:
            result["server_sessions"].append({"event": match[1], "player": int(match[2]),
                                               "active": int(match[3]), "utc": time})
        match = RESPONSE.search(line)
        if match:
            result["responses"].append({"stage": stage, "utc": time, "operation": match[1], "result": match[2],
                "rate_limited": int(match[3]), "accepted": int(match[4]), "credited": int(match[5])})
        match = re.search(r"Dump Self=(-?\d+) Authority=(\d) Run=(.*?) Quota=(.*)$", line)
        if match:
            snapshot = {"stage": stage, "utc": time, "self": int(match[1]), "authority": int(match[2]),
                        "quota": unpack(match[4]), "players": []}
            result["snapshots"].append(snapshot)
        match = re.search(r"Dump Player=(-?\d+) IsSelf=(\d) Wallet=(.*?) Life=(.*?) RunResult=(.*)$", line)
        if match and snapshot is not None:
            snapshot["players"].append({"id": int(match[1]), "is_self": match[2] == "1", "wallet": unpack(match[3])})
        if "Dump PourView " in line:
            # 벡터는 공백을 포함하므로 다음 이름표 앞까지 읽는다
            values = dict(re.findall(r"(Player|IsSelf|Pawn|Eye|Rotation|Control|StationActor|StationLocation|Station|Inlet|Forward|Hit|HitDistance)=(.*?)(?= (?:Player|IsSelf|Pawn|Eye|Rotation|Control|StationActor|StationLocation|Station|Inlet|Forward|Hit|HitDistance)=|$)", line.split("Dump PourView ", 1)[1]))
            result["views"].append({"stage": stage, "utc": time, **values})
    return result


def observe(directory):
    directory = Path(directory)
    manifest = json.loads((directory / "session.json").read_text(encoding="utf-8-sig"))
    logs = {}
    for entry in manifest["processes"]:
        actions = {int(action["slot"]): action["stage"] for action in manifest["actions"] if action["participant"] == entry["name"]}
        logs[entry["name"]] = parse_log(entry["log"], actions)
        for view in logs[entry["name"]]["views"]:
            view["valid"] = pose_valid(view)
    return manifest, logs


def own(snapshot):
    rows = [row for row in snapshot["players"] if row["is_self"]]
    return rows[0]["wallet"].get("BalanceCredits", 0) if len(rows) == 1 else None


def snapshots_at(log, stage):
    return [snapshot for snapshot in log["snapshots"] if snapshot["stage"] == stage]


def pose_valid(view):
    pawn, eye, inlet, forward, rotation, station = [vector(view.get(key, "")) for key in ("Pawn", "Eye", "Inlet", "Forward", "Rotation", "StationLocation")]
    if any(value is None for value in (pawn, eye, inlet, forward, rotation, station)):
        return False
    if not all(math.isfinite(value) for point in (pawn, eye, inlet, forward, rotation, station) for value in point):
        return False
    subtract = lambda a, b: [x - y for x, y in zip(a, b)]
    dot = lambda a, b: sum(x * y for x, y in zip(a, b))
    distance = lambda value: math.sqrt(dot(value, value))
    to_pawn, to_inlet = subtract(pawn, inlet), subtract(inlet, eye)
    pitch, yaw = map(math.radians, rotation[:2])
    direction = [math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch)]
    if distance(to_pawn) > 200.1 or distance(subtract(pawn, station)) > 200.1 or dot(to_pawn, forward) <= 0:
        return False
    if distance(to_inlet) > 0.01 and dot(direction, to_inlet) / distance(to_inlet) < 0.499:
        return False
    hit = view.get("Hit", "")
    station_actor = view.get("StationActor", "")
    try:
        hit_distance = float(view.get("HitDistance", "-1"))
    except ValueError:
        return False
    return hit == "None" or (bool(station_actor) and hit == station_actor) or hit_distance + 4 >= distance(to_inlet)


def network_window(changes, start, end, expected):
    # 과거 적용 이력에 요청값이 한 번 있다는 사실만으로 붓기 구간을 통과시키지 않음
    if start is None or end is None or end < start or not changes or any(row["utc"] is None for row in changes):
        return False, {"reason": "missing setting UTC or session boundaries", "start": start, "end": end}
    ordered = sorted(changes, key=lambda row: row["utc"])
    before = [row for row in ordered if row["utc"] <= start]
    until_end = [row for row in ordered if row["utc"] <= end]
    during = [row for row in ordered if start <= row["utc"] <= end]
    valid = bool(before) and bool(until_end) and before[-1]["value"] == expected and until_end[-1]["value"] == expected
    valid = valid and all(row["value"] == expected for row in during)
    return valid, {"start": start, "end": end, "expected": expected, "at_start": before[-1:] or None,
                   "at_end": until_end[-1:] or None, "changes_during": during}


def judge(directory):
    manifest, logs = observe(directory)
    checks = []
    def check(name, success, detail=None):
        checks.append({"check": name, "state": "pass" if success else "failed_or_unverified", "detail": detail})
    check("runner completed", manifest.get("status") == "completed", manifest.get("error"))
    check("participant count", len(logs) == manifest["players"] and len(logs) in (1, 4))
    check("engine version", manifest.get("engine_version") == "5.8.4")
    check("original settings preserved", manifest.get("user_ini_before") == manifest.get("user_ini_after"))
    check("tracked config preserved", manifest.get("config_before") == manifest.get("config_after"))
    check("module and map unchanged", bool(manifest.get("module_before")) and manifest.get("module_before") == manifest.get("module_after")
          and bool(manifest.get("map_before")) and manifest.get("map_before") == manifest.get("map_after"))
    owner_ids = {}
    for action in manifest["actions"]:
        log = logs[action["participant"]]
        actual = [slot for slot in log["slots"] if slot["slot"] == action["slot"]]
        normalized = action["path"].replace("\\", "/").lower()
        loaded = any(normalized in path.replace("\\", "/").lower() for path in log["exec_files"])
        written = dt.datetime.fromisoformat(action["written_utc"]).timestamp() if action.get("written_utc") else math.inf
        check(action["participant"] + " " + action["stage"] + ": command file executed once after write",
              len(actual) == 1 and actual[0]["utc"] >= written and loaded,
              {"slot": action["slot"], "actual": actual, "written": written if math.isfinite(written) else None})
    for entry in manifest["processes"]:
        name, log = entry["name"], logs[entry["name"]]
        check(name + ": ready and orderly exit", log["ready"] and entry.get("exit_code") == 0 and not entry.get("stopped_by_runner", False))
        check(name + ": commands recognized", not log["commands_failed"], log["commands_failed"])
        check(name + ": no gameplay or fatal errors", not log["errors"], log["errors"])
        initial = snapshots_at(log, "initial")
        granted = snapshots_at(log, "granted")
        check(name + ": initial zero and grant observed", bool(initial) and bool(granted) and own(initial[-1]) == 0
              and own(granted[-1]) == entry["grant"])
        self_id = initial[-1]["self"] if initial else None
        owner_ids[name] = self_id
        check(name + ": snapshot authority and stable owner identity", bool(log["snapshots"]) and self_id is not None and self_id >= 0
              and all(snapshot["self"] == self_id and snapshot["authority"] == int(name == "host")
                      and len([player for player in snapshot["players"] if player["is_self"] and player["id"] == self_id]) == 1
                      and len([player for player in snapshot["players"] if player["is_self"]]) == 1
                      for snapshot in log["snapshots"]))
        check(name + ": full roster observed", any(len(row["players"]) == manifest["players"] for row in granted))
        successful_submits = [row for row in log["responses"] if row["operation"] == "Submit" and row["result"] == "Success"]
        accepted_total = sum(row["accepted"] for row in successful_submits)
        final_rows = snapshots_at(log, f"cycle-{manifest['cycles']:02d}-post")
        final_balance = own(final_rows[-1]) if final_rows else None
        check(name + ": grant to final wallet accounting", final_balance is not None
              and entry["grant"] - final_balance == accepted_total,
              {"grant": entry["grant"], "final": final_balance, "accepted_sum": accepted_total})
        continuity = []
        for snapshot in log["snapshots"]:
            if granted and snapshot["utc"] is not None and snapshot["utc"] >= granted[0]["utc"]:
                spent = sum(row["accepted"] for row in successful_submits if row["utc"] is not None and row["utc"] <= snapshot["utc"])
                expected = entry["grant"] - spent
                continuity.append({"stage": snapshot["stage"], "utc": snapshot["utc"], "expected": expected, "actual": own(snapshot)})
        check(name + ": every observed wallet remains continuous after grant", bool(continuity)
              and all(row["expected"] == row["actual"] for row in continuity), continuity)
        if name != "host":
            others = [player for snapshot in log["snapshots"] for player in snapshot["players"] if not player["is_self"]]
            check(name + ": other wallets hidden", bool(others) and all(player["wallet"].get("BalanceCredits", 0) == 0 for player in others))
            check(name + ": exact collective deposits hidden", bool(log["snapshots"]) and all(
                snapshot["quota"].get("Quota", snapshot["quota"]).get("DepositedCredits", 0) == 0 for snapshot in log["snapshots"]))
        prepared = [view for view in log["views"] if view["stage"] == "prepared" and view.get("IsSelf") == "1"]
        check(name + ": prepared viewpoint", bool(prepared) and pose_valid(prepared[-1]), prepared[-1:] or None)
    check("all server teleports succeeded", len(logs["host"]["teleports"]) >= manifest["players"]
          and all(row["success"] for row in logs["host"]["teleports"]), logs["host"]["teleports"])
    server_views = [view for view in logs["host"]["views"] if view["stage"] == "prepared"]
    latest_server_views = {view.get("Player"): view for view in server_views}
    check("all server camera caches prepared", len(latest_server_views) == manifest["players"]
          and all(pose_valid(view) for view in latest_server_views.values()), latest_server_views)
    check("distinct owner identifiers", len(owner_ids) == manifest["players"]
          and None not in owner_ids.values() and len(set(owner_ids.values())) == manifest["players"], owner_ids)
    # 각자의 n번째 세션을 n라운드에 연결하므로 호스트 콘솔 슬롯 순서에 의존하지 않음
    expected_ids = set(owner_ids.values()) - {None}
    active, pending, paired, event_failures = set(), {}, {player: [] for player in expected_ids}, []
    server_events = logs["host"]["server_sessions"]
    previous_time = -math.inf
    for event in server_events:
        player, time = event["player"], event["utc"]
        if player not in expected_ids or time is None or time < previous_time:
            event_failures.append({"reason": "unknown player or unordered/missing server UTC", "event": event})
            continue
        previous_time = time
        if event["event"] == "Begin":
            if player in active:
                event_failures.append({"reason": "duplicate active begin", "event": event})
            else:
                active.add(player)
                pending[player] = event
        else:
            if player not in active:
                event_failures.append({"reason": "end without begin", "event": event})
            else:
                active.remove(player)
                begin = pending.pop(player)
                paired[player].append({"begin": begin["utc"], "end": time})
        if len(active) != event["active"]:
            event_failures.append({"reason": "active count disagrees with observed session set", "event": event, "derived_active": len(active)})
    check("authoritative session event sequence", bool(server_events) and not event_failures and not active and not pending, event_failures)
    check("each owner has exactly the requested authoritative sessions", len(paired) == manifest["players"]
          and all(len(rows) == manifest["cycles"] for rows in paired.values()), {str(player): rows for player, rows in paired.items()})
    rounds = []
    for cycle in range(1, manifest["cycles"] + 1):
        prefix = f"cycle-{cycle:02d}"
        rows, begins, ends = {}, [], []
        for name, log in logs.items():
            pre, post = snapshots_at(log, prefix + "-pre"), snapshots_at(log, prefix + "-post")
            responses = [row for row in log["responses"] if row["stage"] == prefix + "-pour"]
            begin = [row for row in responses if row["operation"] == "PourStarted"]
            submit = [row for row in responses if row["operation"] == "Submit"]
            stop = [row for row in responses if row["operation"] == "PourStopped"]
            normal = len(begin) == 1 and len(stop) == 1 and bool(submit) and all(
                row["result"] == "Success" and not row["rate_limited"] for row in responses)
            check(prefix + " " + name + ": begin submit stop", normal, responses)
            accepted, credited = sum(row["accepted"] for row in submit), sum(row["credited"] for row in submit)
            before, after = own(pre[-1]) if pre else None, own(post[-1]) if post else None
            check(prefix + " " + name + ": wallet accounting", before is not None and after is not None
                  and accepted > 0 and credited > 0 and before - after == accepted,
                  {"before": before, "after": after, "accepted": accepted, "credited": credited})
            if len(begin) == 1 and len(stop) == 1 and begin[0]["utc"] is not None and stop[0]["utc"] is not None:
                begins.append(begin[0]["utc"])
                ends.append(stop[0]["utc"])
                check(prefix + " " + name + ": ordered responses", begin[0]["utc"] < stop[0]["utc"] and all(
                    begin[0]["utc"] <= row["utc"] <= stop[0]["utc"] for row in submit))
            if name != "host":
                server_pairs = paired.get(owner_ids.get(name), [])
                server_pair = server_pairs[cycle - 1] if len(server_pairs) >= cycle else None
                bounded = len(begin) == len(stop) == 1 and begin[0]["utc"] is not None and stop[0]["utc"] is not None and server_pair is not None
                start = min(begin[0]["utc"], server_pair["begin"]) if bounded else None
                end = max(stop[0]["utc"], server_pair["end"]) if bounded else None
                for key, requested in [("lag", manifest["packet_lag_ms"]), ("loss", manifest["packet_loss_pct"])]:
                    valid, detail = network_window(log[key + "_changes"], start, end, requested)
                    check(prefix + " " + name + ": " + key + " setting sustained across server and client intervals", valid, detail)
            rows[name] = {"accepted": accepted, "credited": credited, "before": before, "after": after}
        total = sum(row["credited"] for row in rows.values())
        host_pre = snapshots_at(logs["host"], prefix + "-pre")
        host_post = snapshots_at(logs["host"], prefix + "-post")
        host_before = host_pre[-1]["quota"].get("Quota", host_pre[-1]["quota"]) if host_pre else {}
        host_after = host_post[-1]["quota"].get("Quota", host_post[-1]["quota"]) if host_post else {}
        for name, log in logs.items():
            pre, post = snapshots_at(log, prefix + "-pre"), snapshots_at(log, prefix + "-post")
            before = pre[-1]["quota"].get("Quota", pre[-1]["quota"]) if pre else {}
            after = post[-1]["quota"].get("Quota", post[-1]["quota"]) if post else {}
            if name == "host":
                check(prefix + " host: authoritative quota accounting", bool(pre) and bool(post)
                      and before.get("DepositedCredits", 0) == 0 and after.get("DepositedCredits", 0) == total
                      and total > 0 and total < after.get("TargetCredits", 0), {"before": before, "after": after, "credited_sum": total})
            else:
                public_fields = ("Day", "TotalDay", "TargetCredits", "PeriodGeneration")
                public_matches = bool(pre) and bool(post) and bool(host_pre) and bool(host_post) and all(
                    before.get(field, 0) == host_before.get(field, 0) and after.get(field, 0) == host_after.get(field, 0)
                    for field in public_fields)
                check(prefix + " " + name + ": public quota matches and exact deposits hidden", public_matches
                      and before.get("DepositedCredits", 0) == 0 and after.get("DepositedCredits", 0) == 0,
                      {"before": before, "after": after})
        reset = [row for row in logs["host"]["quota_reset"] if row["stage"] == prefix + "-reset"]
        check(prefix + ": quota reset observed", len(reset) == 1 and reset[0]["success"])
        client_skew = max(begins) - min(begins) if len(begins) == manifest["players"] else None
        client_overlap = min(ends) - max(begins) if len(ends) == len(begins) == manifest["players"] else None
        sessions = [pairs[cycle - 1] for pairs in paired.values() if len(pairs) >= cycle]
        server_skew = max(row["begin"] for row in sessions) - min(row["begin"] for row in sessions) if len(sessions) == manifest["players"] else None
        server_overlap = min(row["end"] for row in sessions) - max(row["begin"] for row in sessions) if len(sessions) == manifest["players"] else None
        peak_active = max((event["active"] for event in server_events if sessions and event["utc"] is not None
                           and min(row["begin"] for row in sessions) <= event["utc"] <= max(row["end"] for row in sessions)), default=0)
        check(prefix + ": authoritative synchronized starts", server_skew is not None and server_skew <= 1.0, server_skew)
        check(prefix + ": authoritative common overlap and active participants", server_overlap is not None and server_overlap > 0
              and peak_active == manifest["players"], {"overlap_seconds": server_overlap, "peak_active": peak_active})
        rounds.append({"cycle": cycle, "server_start_skew_seconds": server_skew, "server_common_overlap_seconds": server_overlap,
                       "server_peak_active": peak_active, "client_response_start_skew_seconds": client_skew,
                       "client_response_common_overlap_seconds": client_overlap, "participants": rows})
    return {"passed": all(row["state"] == "pass" for row in checks), "checks": checks, "rounds": rounds,
            "environment_startup_errors": {name: log["startup_errors"] for name, log in logs.items()},
            "limits": ["Local separate-process console/RPC validation; no actual UI input or M3 claim.",
                       "Packet simulation settings are observed; actual dropped packet counts are not measured.",
                       "RNG seeds are uncontrolled."]}


def self_test():
    # 합성 로그는 실제 게임 검증과 구분하며 실패를 통과시키는 회귀만 검사
    stamp = "[2026.11.06-01.02.03:000][  1]"
    with tempfile.TemporaryDirectory(prefix="digether-pour-judge-") as temporary:
        path = Path(temporary) / "game.log"
        path.write_text(stamp + 'LogDigetherFramework: AutoCmd 실행: exec "C:/Binaries/slot-003.txt"\n'
                        + stamp + 'LogDigetherFramework: 납부 결과: Operation=EDigetherDepositOperation::PourStarted Result=EDigetherEconomyResult::PourInteractionLost RateLimited=0 Accepted=0 Credited=0\n', encoding="utf-8")
        parsed = parse_log(path, {3: "cycle-01-pour"})
        assert parsed["responses"][0]["result"] == "PourInteractionLost"
        assert parsed["responses"][0]["stage"] == "cycle-01-pour"
        assert len(parsed["slots"]) == 1
        path.write_text(stamp + "Command not recognized: BugItGo\n" + stamp + "Can't find file 'slot-1.txt'\n", encoding="utf-8")
        assert len(parse_log(path, {})["commands_failed"]) == 2
        assert unpack("(Quota=(DepositedCredits=12,TargetCredits=100))")["Quota"]["DepositedCredits"] == 12
        base = {"Pawn": "X=120 Y=0 Z=100", "Eye": "X=120 Y=0 Z=100", "Rotation": "P=0 Y=180 R=0",
                "StationLocation": "X=0 Y=0 Z=0", "Inlet": "X=0 Y=0 Z=100", "Forward": "X=1 Y=0 Z=0", "Hit": "None"}
        assert pose_valid(base)
        assert not pose_valid({**base, "Pawn": "X=-170 Y=0 Z=100"})
        assert not pose_valid({**base, "Rotation": "P=0 Y=0 R=0"})
        assert not pose_valid({**base, "Hit": "Wall", "HitDistance": "30"})
        fixture = Path(temporary) / "run"
        fixture.mkdir()
        names = ["host", "client1", "client2", "client3"]
        manifest = {"players": 4, "cycles": 1, "engine_version": "5.8.4", "status": "completed", "packet_lag_ms": 100,
                    "packet_loss_pct": 5, "user_ini_before": "a", "user_ini_after": "a", "config_before": "b", "config_after": "b",
                    "module_before": "c", "module_after": "c", "map_before": "d", "map_after": "d", "actions": [], "processes": []}
        epoch = dt.datetime(2026, 11, 6, tzinfo=dt.timezone.utc)
        def timestamp(seconds):
            return "[" + (epoch + dt.timedelta(seconds=seconds)).strftime("%Y.%m.%d-%H.%M.%S:%f")[:-3] + "][  1]"
        for participant, name in enumerate(names):
            lines = [timestamp(0) + "LogDigetherFramework: DigetherAutoCmds 예약: 20개"]
            log_path = fixture / (name + ".log")
            manifest["processes"].append({"name": name, "grant": 5000, "log": str(log_path), "exit_code": 0, "stopped_by_runner": False})
            if participant:
                lines += [timestamp(0) + "LogNet: PktLag set to 0", timestamp(0) + "LogNet: PktLoss set to 0",
                          timestamp(.1) + "LogNet: PktLag set to 100", timestamp(.1) + "LogNet: PktLoss set to 5"]
            def action(slot, seconds, stage):
                file = f"C:/Binaries/{name}/slot-{slot:03d}.txt"
                manifest["actions"].append({"participant": name, "stage": stage, "slot": slot, "path": file,
                                            "written_utc": (epoch + dt.timedelta(seconds=seconds - 1)).isoformat()})
                lines.extend([timestamp(seconds) + "LogDigetherFramework: AutoCmd 실행: exec " + file,
                              timestamp(seconds) + "LogPlayerManagement: Execing " + file])
            def dump(seconds, stage, balance, deposited):
                action(int(seconds), seconds, stage)
                lines.append(timestamp(seconds) + f"LogDigetherFramework: Dump Self={participant + 1} Authority={int(not participant)} Run=() Quota=(Quota=(TargetCredits=100,DepositedCredits={deposited}))")
                for other in range(4):
                    amount = balance if other == participant else (5000 if not participant and stage != "initial" else 0)
                    lines.append(timestamp(seconds) + f"LogDigetherFramework: Dump Player={other + 1} IsSelf={int(other == participant)} Wallet=(BalanceCredits={amount}) Life=() RunResult=()")
            dump(1, "initial", 0, 0)
            dump(2, "granted", 5000, 0)
            action(3, 3, "teleport")
            if not participant:
                for other in range(4):
                    lines.append(timestamp(3) + f"LogDigetherFramework: DigetherTeleportToDeposit 서버 결과: Player={other + 1} Moved=1 Target=X=120 Y=0 Z=100")
            action(4, 4, "prepared")
            for other in range(4) if not participant else [participant]:
                lines.append(timestamp(4) + f"LogDigetherFramework: Dump PourView Player={other + 1} IsSelf={int(other == participant)} Pawn=X=120 Y=0 Z=100 Eye=X=120 Y=0 Z=100 Rotation=P=0 Y=180 R=0 Control=P=0 Y=180 R=0 Station=GameTestDeposit StationActor=Station_C_0 StationLocation=X=0 Y=0 Z=0 Inlet=X=0 Y=0 Z=100 Forward=X=1 Y=0 Z=0 Hit=None HitDistance=-1.000")
            if not participant:
                action(5, 5, "cycle-01-reset")
                lines.append(timestamp(5) + "LogDigetherFramework: DigetherSetQuotaProgress 서버 결과: Applied=1")
            dump(6, "cycle-01-pre", 5000, 0)
            action(8, 8 + participant * .05, "cycle-01-pour")
            for when, operation, accepted, credited in [(8 + participant * .05, "PourStarted", 0, 0),
                    (8.3 + participant * .05, "Submit", 10, 3), (9 + participant * .05, "PourStopped", 0, 0)]:
                lines.append(timestamp(when) + f"LogDigetherFramework: 납부 결과: Operation=EDigetherDepositOperation::{operation} Result=EDigetherEconomyResult::Success RateLimited=0 Station=GameTestDeposit Accepted={accepted} Credited={credited}")
            dump(10, "cycle-01-post", 4990, 0 if participant else 12)
            if not participant:
                for other in range(4):
                    lines.append(timestamp(8 + other * .05) + f"LogDigetherFramework: Pour session: Event=Begin Player={other + 1} Active={other + 1}")
                    lines.append(timestamp(9 + other * .05) + f"LogDigetherFramework: Pour session: Event=End Player={other + 1} Active={3 - other}")
                lines.sort(key=lambda line: utc(STAMP.search(line)[1]))
            log_path.write_text("\n".join(lines), encoding="utf-8")
        session = fixture / "session.json"
        session.write_text(json.dumps(manifest), encoding="utf-8")
        positive = judge(fixture)
        assert positive["passed"], [row for row in positive["checks"] if row["state"] != "pass"]
        client = fixture / "client1.log"
        original = client.read_text(encoding="utf-8")
        continuity_mutation = "\n".join(
            line.replace("BalanceCredits=5000", "BalanceCredits=5005")
            if timestamp(6) in line and "Dump Player=2 IsSelf=1 " in line
            else line.replace("BalanceCredits=4990", "BalanceCredits=4995")
            if timestamp(10) in line and "Dump Player=2 IsSelf=1 " in line else line
            for line in original.splitlines())
        mutations = [
            original.replace("::PourStopped", "::OtherOperation"),
            original.replace("::Success RateLimited=0", "::PourInteractionLost RateLimited=0"),
            original.replace("BalanceCredits=4990", "BalanceCredits=4980"),
            original.replace("DepositedCredits=0", "DepositedCredits=12"),
            original.replace("TargetCredits=100", "TargetCredits=200"),
            original.replace("PktLag set to 100", "PktLag set to 0"),
            original.replace("PktLoss set to 5", "PktLoss set to 0"),
            original.replace("Wallet=(BalanceCredits=0)", "Wallet=(BalanceCredits=10)"),
            original.replace("Pawn=X=120", "Pawn=X=-120"),
            original.replace("00.00.08:050", "00.00.09:500"),
            original + "\n" + timestamp(11) + "Command not recognized: bad-command",
            continuity_mutation,
            original.replace("Authority=0", "Authority=1"),
            original.replace("Dump Self=2 Authority=0", "Dump Self=99 Authority=0"),
            original + "\n" + timestamp(7) + "LogNet: PktLag set to 0",
            original + "\n" + timestamp(7) + "LogNet: PktLoss set to 0",
            original + "\n" + timestamp(8.5) + "LogNet: PktLag set to 0",
            original + "\n" + timestamp(8.5) + "LogNet: PktLoss set to 0",
            original + "\n" + timestamp(8.5) + "LogNet: PktLag set to 0\n" + timestamp(8.6) + "LogNet: PktLag set to 100",
            original + "\n" + timestamp(8.5) + "LogNet: PktLoss set to 0\n" + timestamp(8.6) + "LogNet: PktLoss set to 5",
        ]
        for number, mutation in enumerate(mutations):
            client.write_text(mutation, encoding="utf-8")
            assert not judge(fixture)["passed"], f"negative fixture {number} was accepted"
        client.write_text(original, encoding="utf-8")
        server = fixture / "host.log"
        server_original = server.read_text(encoding="utf-8")
        sequential_server = [line for line in server_original.splitlines() if "Pour session:" not in line]
        for other in range(4):
            sequential_server.append(timestamp(8 + other * .2) + f"LogDigetherFramework: Pour session: Event=Begin Player={other + 1} Active=1")
            sequential_server.append(timestamp(8.1 + other * .2) + f"LogDigetherFramework: Pour session: Event=End Player={other + 1} Active=0")
        sequential_server.sort(key=lambda line: utc(STAMP.search(line)[1]))
        for mutation in [
            server_original.replace("Event=Begin Player=4 Active=4", "Event=Begin Player=4 Active=3"),
            "\n".join(line for line in server_original.splitlines() if "Pour session:" not in line),
            server_original.replace("Event=End Player=2", "Event=End Player=99"),
            "\n".join(sequential_server),
        ]:
            server.write_text(mutation, encoding="utf-8")
            assert not judge(fixture)["passed"], "invalid authoritative session evidence was accepted"
        server.write_text(server_original, encoding="utf-8")
        manifest["module_after"] = "changed"
        session.write_text(json.dumps(manifest), encoding="utf-8")
        assert not judge(fixture)["passed"]
        manifest["module_after"] = manifest["module_before"]
        manifest["packet_lag_ms"] = 0
        manifest["packet_loss_pct"] = 0
        session.write_text(json.dumps(manifest), encoding="utf-8")
        for name in names[1:]:
            target = fixture / (name + ".log")
            target.write_text(target.read_text(encoding="utf-8").replace("PktLag set to 100", "PktLag set to 0")
                              .replace("PktLoss set to 5", "PktLoss set to 0"), encoding="utf-8")
        assert judge(fixture)["passed"], "explicit zero network settings must be observable and accepted"
        zero_original = client.read_text(encoding="utf-8")
        for line in [timestamp(7) + "LogNet: PktLag set to 100", timestamp(8.5) + "LogNet: PktLoss set to 5"]:
            client.write_text(zero_original + "\n" + line, encoding="utf-8")
            assert not judge(fixture)["passed"], "zero settings changed before/during a session were accepted"
        assert not network_window([], 8, 9, 0)[0]
        assert not network_window([{"utc": None, "value": 0}], 8, 9, 0)[0]
    return {"self_test": "passed", "full_positive_fixtures": 2, "full_negative_fixtures": 27,
            "scope": "synthetic parser/accounting/overlap/lag-loss/privacy/pose fixtures; no Unreal execution"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?")
    parser.add_argument("--observe", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        print(json.dumps(self_test(), ensure_ascii=False))
        return 0
    if not args.directory:
        parser.error("directory is required")
    if args.observe:
        _, logs = observe(args.directory)
        print(json.dumps(logs, ensure_ascii=True))
        return 0
    report = judge(args.directory)
    output = Path(args.directory) / "judgement.json"
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "checks": len(report["checks"]),
                      "failed": [row for row in report["checks"] if row["state"] != "pass"], "report": str(output)}, ensure_ascii=False))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
