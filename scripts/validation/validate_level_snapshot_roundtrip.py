"""LevelSnapshots 실제 복원 + 디스크 왕복 검증 (Unreal Python, 2-phase).

반드시 서로 다른 새 풀 에디터 프로세스에서 capture -> restore 순서로 실행한다.
Commandlet(-run=pythonscript)은 사용하지 않는다: 스냅샷 적용 시 추가 액터 제거 핸들러
(LevelSnapshots WorldDataUtil.cpp)가 GUnrealEd->DeleteActors 를 쓰는데, Commandlet은 GEngine=GEditor
만 만들고 GUnrealEd 를 만들지 않아 ensure 가 실패하며 액터가 제거되지 않는다. 풀 에디터만 GUnrealEd 를 가진다.

마운트 수명: 픽스처 루트 마운트는 이 격리 에디터 프로세스가 끝날 때까지 유지하고 스크립트에서
UnregisterMountPoint 를 호출하지 않는다. 저장 시 검증(DataValidation)이 다음 틱에 지연 실행되므로
그 전에 루트를 해제하면 Localization 검증기가 경로를 해석하지 못한다. 마운트 등록은 프로세스 로컬이며
영속 설정이 아니므로 프로세스 종료 시 OS 가 해제한다. 사용자의 기존 에디터에는 격리 가드로 실행되지 않는다.

논리 루트: 이 픽스처의 /Game/DigetherSnapshotQA<16hex>/ 중첩 마운트는 이미 등록된 ProjectContent 도메인을
상속하지만, 임의의 최상위 마운트에는 자동으로 맞는 도메인이 없으므로 픽스처 루트를 /Game/ 아래에 중첩 등록한다
(/Game/ 자체는 건드리지 않는다). 패키지 이름은 /Game 으로
시작하지만 중첩 마운트는 물리적으로 이 실행의 run-dir/Content(Saved 하위)에만 쓴다. 어떤 저장·복제보다 먼저
unreal.PackageTools.package_name_to_filename 으로 map/snapshot 파일 해석 결과가 예상 경로와 정확히 같음을
증명하고, 다르거나 API 가 없으면 에셋 쓰기 전에 실패한다. 해석된 경로는 로그와 JSON 에 남긴다.
부모가 공식 -UserDir=<무시되는 Saved/DeveloperChecks/run/EditorUser/> 로 AssetSearch DB 를 격리하면 실효
unreal.Paths.project_saved_dir() 가 <UserDir>/Saved/ 가 되므로 --run-dir 은 그 아래(예: <UserDir>/Saved/SnapshotRoundtrip)에
있어야 한다. 기존 동적 Saved 가드가 이를 그대로 처리한다.

  UnrealEditor-Cmd.exe <DIGETHER.uproject> ^
    -ExecutePythonScript="D:/DIGETHER/scripts/validation/validate_level_snapshot_roundtrip.py --phase capture --run-dir <abs Saved 하위 고유 폴더> --run-id <id> --allow-isolated-editor" ^
    -ScriptErrorsAreFatal -unattended -nullrhi -nosound -nop4
  (같은 --run-dir/--run-id 로 --phase restore 를 새 프로세스에서 한 번 더 실행)

-ScriptErrorsAreFatal 은 필수다: 이것이 있어야 스크립트 예외가 Fatal 로그가 되어 프로세스가
0 이 아닌 값으로 끝난다. -ExecutePythonScript 시작 경로는 스크립트 완료 후 다음 틱에
에디터 종료(QUIT_EDITOR)를 스스로 요청하므로 이 스크립트는 quit_editor 를 호출하지 않는다.
사용자가 이미 띄운 에디터 안에서는 실행하지 않는다.

인자 (환경변수 기본값):
  --phase                     DIGETHER_SNAPSHOT_PHASE   capture | restore
  --run-dir                   DIGETHER_SNAPSHOT_RUN_DIR 절대경로, project Saved 하위여야 함
  --run-id                    DIGETHER_SNAPSHOT_RUN_ID  부모가 정한 고유 id
  --allow-isolated-editor     DIGETHER_SNAPSHOT_ALLOW_ISOLATED=1  격리 실행 동의

산출물 (run-dir): capture_manifest.json, restore_result.json, failure_<phase>.json,
Content/Maps/Lvl_SnapshotFixture.umap, Content/Snapshots/LS_SnapshotFixture.uasset.
성공 마커: DIGETHER_SNAPSHOT_CAPTURE_OK / DIGETHER_SNAPSHOT_RESTORE_OK (run-id 포함).
실패는 예외를 다시 던지며, -ScriptErrorsAreFatal 덕분에 에디터 프로세스가 0 이 아닌 값으로 끝난다.
check_baseline 실패 시 전체 실패 목록·실제 상태·속성 차이를 담은 진단 JSON 을 로그와 failure 파일에 남긴다.

한계: 기존 파일을 삭제/덮어쓰지 않는다(JSON은 'x' 모드). 소유한 패키지만 저장한다.
스냅샷은 headless 안전 경로로 만든다: LevelSnapshotsFunctionLibrary.take_level_snapshot 으로
transient 스냅샷을 캡처하고, AssetTools.duplicate_asset 으로 고유 대상에 영구 에셋을 만든 뒤
EditorAssetSubsystem.save_loaded_asset 으로 저장한다(Slate 알림/뷰포트 썸네일 미사용).
실제 복원은 영구 에셋으로 수행한다.
"""

import argparse
import json
import os
import re
import traceback
import uuid
from pathlib import Path

try:
    import unreal
except ImportError:
    unreal = None

MAP_NAME = "Lvl_SnapshotFixture"
SNAP_NAME = "LS_SnapshotFixture"
CUBE = "/Engine/BasicShapes/Cube.Cube"
TOL = 1e-3
ACTOR_A = "SnapKeep"
ACTOR_B = "SnapRemoved"
ACTOR_EXTRA = "SnapExtra"
MARKERS = {
    "capture": "DIGETHER_SNAPSHOT_CAPTURE_OK",
    "restore": "DIGETHER_SNAPSHOT_RESTORE_OK",
}

_stage = ["init"]
_diagnostics = []


class QAError(RuntimeError):
    pass


def stage(name):
    _stage[0] = name
    log("stage: " + name)


def log(msg):
    print("[SnapshotQA] " + msg)
    if unreal:
        unreal.log("[SnapshotQA] " + msg)


def need(cond, msg):
    if not cond:
        raise QAError(msg)


def parse_args(argv=None):
    env = os.environ.get
    p = argparse.ArgumentParser(description="LevelSnapshots roundtrip fixture")
    p.add_argument("--phase", choices=["capture", "restore"], default=env("DIGETHER_SNAPSHOT_PHASE"))
    p.add_argument("--run-dir", default=env("DIGETHER_SNAPSHOT_RUN_DIR"))
    p.add_argument("--run-id", default=env("DIGETHER_SNAPSHOT_RUN_ID"))
    p.add_argument(
        "--allow-isolated-editor",
        action="store_true",
        default=env("DIGETHER_SNAPSHOT_ALLOW_ISOLATED") == "1",
    )
    args = p.parse_args(argv)
    if not args.phase or not args.run_dir or not args.run_id:
        p.error("--phase, --run-dir, --run-id are required")
    return args


# ---------- paths / safety ----------

def resolve_run_dir(raw):
    need(os.path.isabs(raw), "run-dir must be absolute: " + raw)
    saved = os.path.realpath(unreal.Paths.project_saved_dir())
    run = os.path.realpath(raw)
    need(
        os.path.normcase(os.path.commonpath([saved, run])) == os.path.normcase(saved)
        and os.path.normcase(saved) != os.path.normcase(run),
        "run-dir must be strictly under project Saved: %s (saved=%s)" % (run, saved),
    )
    cur = os.path.abspath(raw)
    junction = getattr(os.path, "isjunction", lambda _p: False)
    while os.path.normcase(os.path.realpath(cur)) != os.path.normcase(saved) and cur != os.path.dirname(cur):
        need(not os.path.islink(cur) and not junction(cur), "link in run-dir path: " + cur)
        cur = os.path.dirname(cur)
    need(os.path.normcase(os.path.abspath(raw)) == os.path.normcase(run), "run-dir is not canonical: %s -> %s" % (raw, run))
    return Path(run)


def require_isolated(args):
    need(args.allow_isolated_editor, "missing --allow-isolated-editor opt-in")
    cmd = unreal.SystemLibrary.get_command_line().lower()
    need("-executepythonscript=" in cmd, "not started via -ExecutePythonScript=; refusing")
    need("-scripterrorsarefatal" in cmd, "-ScriptErrorsAreFatal required for nonzero exit on failure; refusing")
    need("-run=" not in cmd, "commandlet (-run=) has no GUnrealEd; snapshot removal needs a full Editor; refusing")


def require_clean_editor():
    util = unreal.EditorLoadingAndSavingUtils
    dirty = [str(p.get_name()) for p in list(util.get_dirty_map_packages()) + list(util.get_dirty_content_packages())]
    need(not dirty, "dirty packages loaded, refusing destructive map switch: %s" % dirty)


def write_json_new(path, data):
    path = Path(path)
    with open(path, "x", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    return path


def layout(run_dir, mount):
    content = run_dir / "Content"
    root = "/Game/%s" % mount
    return {
        "content_dir": str(content),
        "mount_root": root + "/",
        "map_pkg": "%s/Maps/%s" % (root, MAP_NAME),
        "snap_folder": "%s/Snapshots" % root,
        "snap_pkg": "%s/Snapshots/%s" % (root, SNAP_NAME),
        "map_file": content / "Maps" / (MAP_NAME + ".umap"),
        "snap_file": content / "Snapshots" / (SNAP_NAME + ".uasset"),
    }


# ---------- mount / world ----------

def editor_world():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    need(w is not None, "no editor world")
    return w


def mount_cmd(verb, mount_root, content_dir):
    cmd = 'PackageName.%s %s "%s"' % (verb, mount_root, content_dir.replace("\\", "/"))
    unreal.SystemLibrary.execute_console_command(editor_world(), cmd)


def _canon(p):
    return os.path.normcase(os.path.realpath(str(p)))


def verify_mount_mapping(lay):
    """저장 전 증명: 중첩 마운트가 이 실행의 Saved 하위 파일로만 해석되는지 확인한다."""
    stage("verify_mount_mapping")
    pt = getattr(unreal, "PackageTools", None)
    need(pt is not None and callable(getattr(pt, "package_name_to_filename", None)),
         "unreal.PackageTools.package_name_to_filename not exposed; cannot prove mount mapping")
    saved = _canon(unreal.Paths.project_saved_dir())
    content = _canon(lay["content_dir"])
    need(os.path.commonpath([saved, content]) == saved and content != saved,
         "fixture Content is not under effective project Saved: %s (saved=%s)" % (content, saved))
    resolved = {}
    for key, pkg, ext, expected in (("map", lay["map_pkg"], ".umap", lay["map_file"]),
                                    ("snapshot", lay["snap_pkg"], ".uasset", lay["snap_file"])):
        raw = pt.package_name_to_filename(pkg, ext)
        need(isinstance(raw, str) and raw.strip(), "empty file mapping for %s (%s)" % (key, pkg))
        got, want = _canon(raw), _canon(expected)
        need(got == want, "%s mount mapping mismatch: %s -> %r (resolved %s) != expected %s" % (key, pkg, raw, got, want))
        need(os.path.commonpath([saved, got]) == saved and got != saved,
             "%s resolved path escapes effective Saved: %s" % (key, got))
        resolved[key] = got
    log("mount mapping verified: map=%s snapshot=%s" % (resolved["map"], resolved["snapshot"]))
    return resolved


def sizes_ok(path):
    return os.path.isfile(path) and os.path.getsize(path) > 0


# ---------- actor fixture ----------

def actor_ss():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_actors():
    return {a.get_name(): a for a in actor_ss().get_all_level_actors() if isinstance(a, unreal.StaticMeshActor)}


def spawn(name, label, tag, loc, rot, scale, mesh, hidden, cast_shadow):
    a = actor_ss().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    need(a is not None, "spawn failed: " + name)
    need(a.rename(name) is True, "rename returned false: " + name)
    need(a.get_name() == name, "rename mismatch: %s != %s" % (a.get_name(), name))
    a.set_actor_label(label)
    a.set_editor_property("tags", [unreal.Name(tag)])
    comp = a.get_editor_property("static_mesh_component")
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(mesh)
    apply_transform(a, loc, rot, scale)
    a.set_actor_hidden_in_game(hidden)
    comp.set_editor_property("cast_shadow", cast_shadow)
    return a


def apply_transform(a, loc, rot, scale):
    a.set_actor_location(unreal.Vector(*loc), False, False)
    a.set_actor_rotation(unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]), False)
    a.set_actor_scale3d(unreal.Vector(*scale))


def read_state(a):
    comp = a.get_editor_property("static_mesh_component")
    loc, rot, sc = a.get_actor_location(), a.get_actor_rotation(), a.get_actor_scale3d()
    mesh = comp.get_editor_property("static_mesh")
    return {
        "name": a.get_name(),
        "label": a.get_actor_label(),
        "location": [loc.x, loc.y, loc.z],
        "rotation": [rot.roll, rot.pitch, rot.yaw],
        "scale": [sc.x, sc.y, sc.z],
        "hidden_in_game": bool(a.get_editor_property("hidden")),
        "cast_shadow": bool(comp.get_editor_property("cast_shadow")),
        "mesh": mesh.get_path_name() if mesh else None,
        "mobility": str(comp.get_editor_property("mobility")),
        "tags": [str(t) for t in a.get_editor_property("tags")],
    }


def _angle_close(x, y):
    return abs((x - y + 180.0) % 360.0 - 180.0) <= TOL


def diff_state(cur, base):
    d = []
    for k in ("label", "hidden_in_game", "cast_shadow", "mesh", "mobility", "tags"):
        if cur[k] != base[k]:
            d.append(k)
    for k in ("location", "scale"):
        if any(abs(x - y) > TOL for x, y in zip(cur[k], base[k])):
            d.append(k)
    if not all(_angle_close(x, y) for x, y in zip(cur["rotation"], base["rotation"])):
        d.append("rotation")
    return d


def make_baseline():
    mesh = unreal.load_asset(CUBE)
    need(mesh is not None, "missing asset " + CUBE)
    spawn(ACTOR_A, "QA Keep", "QA_Keep", (100.0, 200.0, 50.0), (10.0, 20.0, 30.0), (1.5, 2.0, 0.5), mesh, False, True)
    spawn(ACTOR_B, "QA Removed", "QA_Removed", (-300.0, 120.0, 80.0), (0.0, 0.0, 75.0), (0.75, 1.25, 2.0), mesh, True, False)
    actors = find_actors()
    return {n: read_state(actors[n]) for n in (ACTOR_A, ACTOR_B)}


def check_baseline(baseline, where):
    actors = find_actors()
    failures = []
    states = {}
    prop_diffs = {}
    extra_present = ACTOR_EXTRA in actors
    if extra_present:
        failures.append("unexpected extra actor present: " + ACTOR_EXTRA)
    missing = [n for n in baseline if n not in actors]
    for n in missing:
        failures.append("actor missing: " + n)
    for name, base in baseline.items():
        if name not in actors:
            continue
        cur = read_state(actors[name])
        states[name] = cur
        d = diff_state(cur, base)
        if d:
            prop_diffs[name] = {k: {"actual": cur[k], "expected": base[k]} for k in d}
            failures.append("%s differs from baseline: %s" % (name, d))
    if extra_present:
        states[ACTOR_EXTRA] = read_state(actors[ACTOR_EXTRA])
    if failures:
        diag = {
            "where": where,
            "extra_actor_present": extra_present,
            "missing_expected_actors": missing,
            "present_static_mesh_actors": sorted(actors),
            "actual_states": states,
            "property_differences": prop_diffs,
            "failures": failures,
        }
        _diagnostics.append(diag)
        log("CHECK_BASELINE_DIAGNOSTIC " + json.dumps(diag, ensure_ascii=False))
    need(not failures, "%s: baseline check failed: %s" % (where, failures))


def mutate_and_assert(baseline):
    actors = find_actors()
    a = actors[ACTOR_A]
    apply_transform(a, (900.0, -450.0, 333.0), (-15.0, 40.0, 120.0), (3.0, 0.4, 2.2))
    a.set_actor_label("QA Keep Mutated")
    a.set_actor_hidden_in_game(True)
    a.get_editor_property("static_mesh_component").set_editor_property("cast_shadow", False)
    need(actor_ss().destroy_actor(actors[ACTOR_B]), "destroy_actor failed")
    mesh = unreal.load_asset(CUBE)
    spawn(ACTOR_EXTRA, "QA Extra", "QA_Extra", (0.0, 0.0, 700.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0), mesh, False, True)
    now = find_actors()
    need(ACTOR_B not in now and ACTOR_EXTRA in now, "mutation not visible in world")
    d = diff_state(read_state(now[ACTOR_A]), baseline[ACTOR_A])
    for k in ("location", "rotation", "scale", "label", "hidden_in_game", "cast_shadow"):
        need(k in d, "mutation did not change %s before restore" % k)
    return d


def apply_and_verify(snapshot, baseline):
    stage("apply_snapshot")
    unreal.LevelSnapshotsFunctionLibrary.apply_snapshot_to_world(editor_world(), snapshot, None)
    stage("verify_restored")
    check_baseline(baseline, "after apply")


def check_snapshot(snapshot, lay):
    need(snapshot is not None, "snapshot is None")
    need(isinstance(snapshot, unreal.LevelSnapshot), "wrong snapshot class: %s" % type(snapshot).__name__)
    get_map_path = getattr(snapshot, "get_map_path", None)
    need(get_map_path is not None, "LevelSnapshot.get_map_path not exposed")
    # PythonScriptPlugin hides native Break/Make UFunctions (not on SystemLibrary); structs expose
    # the native break via StructBase.to_tuple. SoftObjectPath breaks into one FString.
    soft_path = get_map_path()
    need(callable(getattr(soft_path, "to_tuple", None)), "SoftObjectPath.to_tuple not exposed")
    parts = soft_path.to_tuple()
    need(isinstance(parts, tuple) and len(parts) == 1 and isinstance(parts[0], str),
         "SoftObjectPath.to_tuple() must be a 1-string tuple, got %r" % (parts,))
    mp = parts[0]
    expected = "%s.%s" % (lay["map_pkg"], MAP_NAME)
    need(mp == expected, "snapshot map_path %r != expected %r" % (mp, expected))
    world_path = editor_world().get_path_name()
    need(mp == world_path, "snapshot map_path %r != editor world %r" % (mp, world_path))
    return mp


def persist_snapshot(transient, lay):
    stage("persist_snapshot")
    need(not lay["snap_file"].exists(), "snapshot file already exists")
    eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    need(not eas.does_asset_exist(lay["snap_pkg"]), "snapshot asset already registered: " + lay["snap_pkg"])
    persistent = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(SNAP_NAME, lay["snap_folder"], transient)
    need(persistent is not None, "duplicate_asset returned None")
    need(isinstance(persistent, unreal.LevelSnapshot), "duplicate is wrong class: %s" % type(persistent).__name__)
    need(persistent is not transient, "duplicate_asset returned the transient object")
    need(persistent.get_path_name() == "%s.%s" % (lay["snap_pkg"], SNAP_NAME),
         "persistent snapshot path mismatch: " + persistent.get_path_name())
    need(eas.save_loaded_asset(persistent, False) is True, "save_loaded_asset (snapshot) returned false")
    need(sizes_ok(lay["snap_file"]), "snapshot .uasset missing/empty: %s" % lay["snap_file"])
    return persistent


# ---------- phases ----------

def run_capture(args, run_dir, state):
    stage("preflight")
    require_isolated(args)
    require_clean_editor()
    for n in ("capture_manifest.json", "restore_result.json"):
        need(not (run_dir / n).exists(), "output already exists: " + n)
    mount = "DigetherSnapshotQA" + uuid.uuid4().hex[:16]
    lay = layout(run_dir, mount)
    need(not lay["map_file"].exists() and not lay["snap_file"].exists(), "fixture outputs already exist")
    os.makedirs(lay["map_file"].parent, exist_ok=True)
    os.makedirs(lay["snap_file"].parent, exist_ok=True)

    stage("mount")
    state["mount"], state["content_dir"], state["mount_root"] = mount, lay["content_dir"], lay["mount_root"]
    mount_cmd("RegisterMountPoint", lay["mount_root"], lay["content_dir"])
    state["mounted"] = True
    resolved = verify_mount_mapping(lay)

    stage("new_blank_map")
    need(unreal.EditorLoadingAndSavingUtils.new_blank_map(False) is not None, "new_blank_map failed")
    baseline = make_baseline()

    stage("save_baseline_map")
    need(unreal.EditorLoadingAndSavingUtils.save_map(editor_world(), lay["map_pkg"]), "save_map (baseline) failed")
    need(sizes_ok(lay["map_file"]), "baseline map file missing/empty")

    stage("take_snapshot")
    transient = unreal.LevelSnapshotsFunctionLibrary.take_level_snapshot(
        editor_world(), SNAP_NAME, "DIGETHER roundtrip fixture"
    )
    map_path = check_snapshot(transient, lay)
    snap = persist_snapshot(transient, lay)
    check_snapshot(snap, lay)

    stage("mutate")
    changed = mutate_and_assert(baseline)
    apply_and_verify(snap, baseline)

    stage("save_restored_map")
    need(unreal.EditorLoadingAndSavingUtils.save_map(editor_world(), lay["map_pkg"]), "save_map (restored) failed")
    need(sizes_ok(lay["map_file"]), "restored map file missing/empty")
    return {
        "mount": mount,
        "mount_root": lay["mount_root"],
        "resolved_map_file": resolved["map"],
        "resolved_snapshot_file": resolved["snapshot"],
        "map_pkg": lay["map_pkg"],
        "snap_pkg": lay["snap_pkg"],
        "map_file": str(lay["map_file"]),
        "snap_file": str(lay["snap_file"]),
        "map_size": os.path.getsize(lay["map_file"]),
        "snap_size": os.path.getsize(lay["snap_file"]),
        "snapshot_map_path": map_path,
        "baseline": baseline,
        "mutation_changed_fields": changed,
        "in_process_restore_ok": True,
    }


def run_restore(args, run_dir, state):
    stage("preflight")
    require_isolated(args)
    require_clean_editor()
    need(not (run_dir / "restore_result.json").exists(), "restore_result.json already exists")
    mf = run_dir / "capture_manifest.json"
    need(mf.is_file(), "capture manifest missing (no capture fallback): " + str(mf))
    man = json.loads(mf.read_text(encoding="utf-8"))
    need(man.get("success") is True and man.get("phase") == "capture", "capture manifest is not a success manifest")
    need(man.get("run_id") == args.run_id, "run_id mismatch with capture manifest")
    mount = man.get("mount")
    need(isinstance(mount, str) and re.fullmatch(r"DigetherSnapshotQA[0-9a-f]{16}", mount), "bad mount in manifest: %r" % (mount,))
    lay = layout(run_dir, mount)
    for key, derived in (("mount_root", lay["mount_root"]),
                         ("map_pkg", lay["map_pkg"]), ("snap_pkg", lay["snap_pkg"]),
                         ("map_file", str(lay["map_file"])), ("snap_file", str(lay["snap_file"]))):
        need(man.get(key) == derived, "manifest %s %r != derived %r (stale/foreign manifest)" % (key, man.get(key), derived))
    need(sizes_ok(lay["map_file"]) and sizes_ok(lay["snap_file"]), "map/snapshot files missing from capture")
    baseline = man["baseline"]

    stage("mount")
    state["mount"], state["content_dir"], state["mount_root"] = mount, lay["content_dir"], lay["mount_root"]
    mount_cmd("RegisterMountPoint", lay["mount_root"], lay["content_dir"])
    state["mounted"] = True
    resolved = verify_mount_mapping(lay)

    stage("load_map")
    world = unreal.EditorLoadingAndSavingUtils.load_map(lay["map_pkg"])
    need(world is not None, "load_map failed: " + lay["map_pkg"])
    check_baseline(baseline, "after disk load_map")

    stage("load_snapshot")
    snap = unreal.load_asset(lay["snap_pkg"])
    map_path = check_snapshot(snap, lay)

    stage("mutate")
    changed = mutate_and_assert(baseline)
    apply_and_verify(snap, baseline)

    stage("save_restored_map")
    need(unreal.EditorLoadingAndSavingUtils.save_map(editor_world(), lay["map_pkg"]), "save_map (restored) failed")
    need(sizes_ok(lay["map_file"]), "restored map file missing/empty")
    return {
        "mount": mount,
        "mount_root": lay["mount_root"],
        "resolved_map_file": resolved["map"],
        "resolved_snapshot_file": resolved["snapshot"],
        "disk_load_success": True,
        "snapshot_loaded_from_disk": True,
        "real_restore_success": True,
        "snapshot_map_path": map_path,
        "mutation_changed_fields": changed,
        "map_size": os.path.getsize(lay["map_file"]),
        "snap_size": os.path.getsize(lay["snap_file"]),
        "reload_after_restore": "not performed (no safe supported API)",
    }


def cleanup(state):
    errs = []
    try:
        if unreal.EditorLoadingAndSavingUtils.new_blank_map(False) is None:
            errs.append("release_world: new_blank_map returned None")
    except Exception as e:
        errs.append("release_world: %r" % e)
    if state.get("mounted"):
        log("retaining fixture mount %s until isolated Editor process exit "
            "(validate-on-save runs deferred on the next tick)" % state["mount_root"])
    return errs


def mount_info(state):
    return {
        "mount_root": state.get("mount_root"),
        "mount_lifetime": "isolated_editor_process",
        "mount_retained_for_deferred_validation": bool(state.get("mounted")),
    }


def main(argv=None):
    args = parse_args(argv)
    need(unreal is not None, "must run inside Unreal Python")
    run_dir = resolve_run_dir(args.run_dir)
    os.makedirs(run_dir, exist_ok=True)
    state = {}
    result, err = None, None
    try:
        result = (run_capture if args.phase == "capture" else run_restore)(args, run_dir, state)
    except BaseException as e:
        err = {"stage": _stage[0], "error": repr(e), "traceback": traceback.format_exc()}
    cleanup_errors = cleanup(state) if state else []

    if err or cleanup_errors:
        fail = {
            "phase": args.phase,
            "run_id": args.run_id,
            "success": False,
            "failure": err,
            "baseline_diagnostics": _diagnostics,
            "cleanup_failed": bool(cleanup_errors),
            "cleanup_errors": cleanup_errors,
        }
        fail.update(mount_info(state))
        name = "failure_%s.json" % args.phase
        try:
            write_json_new(run_dir / name, fail)
        except FileExistsError:
            write_json_new(run_dir / ("failure_%s_%d.json" % (args.phase, os.getpid())), fail)
        log("FAILED: " + json.dumps(fail, ensure_ascii=False)[:2000])
        raise QAError(
            ("cleanup failed: %s; " % cleanup_errors if cleanup_errors else "")
            + ("stage=%s %s" % (err["stage"], err["error"]) if err else "")
        )

    out = {"phase": args.phase, "run_id": args.run_id, "success": True}
    out.update(result)
    out.update(mount_info(state))
    target = "capture_manifest.json" if args.phase == "capture" else "restore_result.json"
    write_json_new(run_dir / target, out)
    log("%s %s %s" % (MARKERS[args.phase], args.run_id, out["mount"]))


if __name__ == "__main__":
    main()
