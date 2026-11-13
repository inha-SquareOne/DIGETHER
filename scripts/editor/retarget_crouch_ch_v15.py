"""공유 폴더의 Lyra 크라우치 애니메이션 7종을 들여와 CH_v15로 리타겟한다(에디터 Python).

실행: 에디터 안 Tools > Execute Python Script, 또는 에디터를 닫고
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4

원본(MM_Unarmed_Crouch_*)은 Lyra 스켈레톤 /Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin을
참조하지만 프로젝트에는 없다. 뼈 구성이 같은 템플릿 스켈레톤
/Game/Characters/Mannequins/Meshes/SK_Mannequin을 그 경로에 임시로 복사해 로드한 뒤
consolidate로 참조를 실제 템플릿 스켈레톤으로 바꾸고 임시 경로는 지운다.

만드는 것:
- /Game/Characters/Mannequins/Anims/Unarmed/Crouch/MM_Unarmed_Crouch_* (원본, 템플릿 스켈레톤)
- /Game/Characters/DwarfV15/Anims/MM_Unarmed_Crouch_*V15 (RTG_Quinn_To_Ch_v15로 리타겟)
이미 있는 결과는 다시 만들지 않는다.
"""

import os
import shutil
import unreal

SHARE_DIR = r"\\Desktop-et6q4vk\공유폴더\언리얼_자료\Characters\Heroes\Mannequin\Animations\Locomotion\Unarmed"
NAMES = [
    "MM_Unarmed_Crouch_Entry", "MM_Unarmed_Crouch_Exit", "MM_Unarmed_Crouch_Idle",
    "MM_Unarmed_Crouch_Walk_Bwd", "MM_Unarmed_Crouch_Walk_Fwd",
    "MM_Unarmed_Crouch_Walk_Left", "MM_Unarmed_Crouch_Walk_Right",
]

CONTENT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
LYRA_ROOT = "/Game/Characters/Heroes"
LYRA_ANIM_DIR = f"{LYRA_ROOT}/Mannequin/Animations/Locomotion/Unarmed"
LYRA_SKELETON = f"{LYRA_ROOT}/Mannequin/Meshes/SK_Mannequin"

SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
SOURCE_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
SOURCE_DIR = "/Game/Characters/Mannequins/Anims/Unarmed/Crouch"

TARGET_MESH = "/Game/Characters/DwarfV15/SK_Ch_v15_Body"
RETARGETER = "/Game/Characters/DwarfV15/Retarget/RTG_Quinn_To_Ch_v15"
ANIM_DIR = "/Game/Characters/DwarfV15/Anims"
SUFFIX = "V15"

EAL = unreal.EditorAssetLibrary
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg: str) -> None:
    unreal.log(f"[retarget_crouch] {msg}")


def load(path: str):
    asset = EAL.load_asset(path)
    if asset is None:
        raise RuntimeError(f"에셋을 찾을 수 없다: {path}")
    return asset


def disk(package: str) -> str:
    return os.path.join(CONTENT, package[len("/Game/"):].replace("/", os.sep) + ".uasset")


def import_sources() -> None:
    todo = [n for n in NAMES if not EAL.does_asset_exist(f"{SOURCE_DIR}/{n}")]
    if not todo:
        log("원본 애니메이션 모두 있음, 건너뜀")
        return
    # 1) Lyra 경로 그대로 파일 복사 + 임시 스켈레톤
    files = []
    for n in todo:
        dst = disk(f"{LYRA_ANIM_DIR}/{n}")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(os.path.join(SHARE_DIR, n + ".uasset"), dst)
        files.append(dst)
    stub = disk(LYRA_SKELETON)
    os.makedirs(os.path.dirname(stub), exist_ok=True)
    shutil.copyfile(disk(SKELETON), stub)
    files.append(stub)
    REGISTRY.scan_files_synchronous(files, True)

    # 2) 로드 후 임시 스켈레톤 참조를 템플릿 스켈레톤으로 합친다
    anims = [load(f"{LYRA_ANIM_DIR}/{n}") for n in todo]
    real = load(SKELETON)
    if not EAL.consolidate_assets(real, [load(LYRA_SKELETON)]):
        raise RuntimeError("스켈레톤 참조 교체 실패")
    for a in anims:
        sk = a.get_editor_property("skeleton")
        if sk is None or sk.get_path_name() != real.get_path_name():
            raise RuntimeError(f"{a.get_name()} 스켈레톤이 {sk}")
        EAL.save_loaded_asset(a, only_if_is_dirty=False)

    # 3) 프로젝트 마네킹 폴더로 옮기고 Lyra 임시 경로를 지운다
    for n in todo:
        if not EAL.rename_asset(f"{LYRA_ANIM_DIR}/{n}", f"{SOURCE_DIR}/{n}"):
            raise RuntimeError(f"이동 실패: {n}")
    EAL.save_directory(SOURCE_DIR, only_if_is_dirty=False, recursive=True)
    leftovers = EAL.list_assets(LYRA_ROOT, recursive=True, include_folder=False)
    log(f"임시 경로 남은 에셋(리다이렉터): {leftovers}")
    EAL.delete_directory(LYRA_ROOT)
    log(f"원본 {len(todo)}개 들여옴: {SOURCE_DIR}")


def retarget() -> list:
    todo = [f"{SOURCE_DIR}/{n}" for n in NAMES if not EAL.does_asset_exist(f"{ANIM_DIR}/{n}{SUFFIX}")]
    if not todo:
        log("리타겟 결과 모두 있음, 건너뜀")
        return []
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", [EAL.find_asset_data(p) for p in todo])
    inputs.set_editor_property("source_mesh", load(SOURCE_MESH))
    inputs.set_editor_property("target_mesh", load(TARGET_MESH))
    inputs.set_editor_property("ik_retarget_asset", load(RETARGETER))
    inputs.set_editor_property("suffix", SUFFIX)
    inputs.set_editor_property("include_referenced_assets", False)
    inputs.set_editor_property("overwrite_existing_files", False)
    inputs.set_editor_property("use_source_path", False)
    inputs.set_editor_property("target_path", ANIM_DIR)
    results = [str(d.package_name) for d in unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)]
    log(f"리타겟 생성 {len(results)}개: {results}")
    EAL.save_directory(ANIM_DIR, only_if_is_dirty=True, recursive=True)
    return results


PARENTS = {"root": None, "pelvis": "root", "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02",
           "spine_04": "spine_03", "spine_05": "spine_04", "neck_01": "spine_05", "head": "neck_01",
           "thigh_l": "pelvis", "calf_l": "thigh_l", "foot_l": "calf_l", "ball_l": "foot_l"}


def component_pos(seq, bone: str, time: float):
    chain = []
    while bone:
        chain.append(bone)
        bone = PARENTS[bone]
    t = unreal.Transform()
    for name in reversed(chain):
        t = unreal.MathLibrary.compose_transforms(unreal.AnimationLibrary.get_bone_pose_for_time(seq, name, time, False), t)
    return t.translation


def report() -> None:
    """원본과 결과의 골반·머리·발 높이를 남겨 웅크림 유지와 바닥 관통을 확인한다."""
    for n in NAMES:
        for path in (f"{SOURCE_DIR}/{n}", f"{ANIM_DIR}/{n}{SUFFIX}"):
            seq = EAL.load_asset(path)
            if seq is None:
                log(f"  없음 {path}")
                continue
            length = seq.get_play_length()
            parts = []
            for time in (0.0, length * 0.5):
                v = {b: component_pos(seq, b, time) for b in ("pelvis", "head", "ball_l")}
                parts.append(" ".join(f"{b}.z={p.z:.0f}" for b, p in v.items()))
            log(f"  {seq.get_name()} {length:.2f}s 스켈레톤={seq.get_editor_property('skeleton').get_name()} "
                f"| t0 {parts[0]} | 중간 {parts[1]}")


CROUCH_BS = f"{ANIM_DIR}/BS_Crouch_V15"
# 앉아 걷기 최고 속도(cm/s), DT_PlayerStats CrouchSpeed와 맞춘다
CROUCH_SPEED = 200.0


def make_crouch_blendspace() -> None:
    """서기 BS_Idle_Walk_RunV15와 같은 축(Direction -180~180, Speed)으로 앉기 Idle + 4방향 걷기 블렌드스페이스를 만든다."""
    axes = [("Direction", -180.0, 180.0, 8), ("Speed", 0.0, CROUCH_SPEED, 4), ("None", 0.0, 100.0, 4)]
    if EAL.does_asset_exist(CROUCH_BS):
        bs = load(CROUCH_BS)
        now = [(str(p.get_editor_property("display_name")), p.get_editor_property("max")) for p in bs.get_editor_property("blend_parameters")]
        if now[:2] == [(a[0], a[2]) for a in axes[:2]]:
            log(f"블렌드스페이스 있음, 건너뜀: {CROUCH_BS}")
            return
        log(f"블렌드스페이스 축이 다름 {now}, 다시 설정")
    else:
        factory = unreal.BlendSpaceFactoryNew()
        factory.set_editor_property("target_skeleton", load(TARGET_MESH).skeleton)
        folder, name = CROUCH_BS.rsplit("/", 1)
        bs = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.BlendSpace, factory)

    # 구조체 배열은 복사본으로 돌아오므로 새 구조체로 통째로 넣는다. 샘플은 축 범위 안이어야 해서 축을 먼저 넣는다
    params = []
    for label, lo, hi, grid in axes:
        p = unreal.BlendParameter()
        p.set_editor_property("display_name", label)
        p.set_editor_property("min", lo)
        p.set_editor_property("max", hi)
        p.set_editor_property("grid_num", grid)
        params.append(p)
    bs.set_editor_property("blend_parameters", params)

    idle = load(f"{ANIM_DIR}/MM_Unarmed_Crouch_Idle{SUFFIX}")
    walk = {d: load(f"{ANIM_DIR}/MM_Unarmed_Crouch_Walk_{d}{SUFFIX}") for d in ("Fwd", "Bwd", "Left", "Right")}
    points = [(idle, x, 0.0) for x in (-180.0, -90.0, 0.0, 90.0, 180.0)]
    points += [(walk["Fwd"], 0.0, CROUCH_SPEED), (walk["Left"], -90.0, CROUCH_SPEED), (walk["Right"], 90.0, CROUCH_SPEED),
               (walk["Bwd"], -180.0, CROUCH_SPEED), (walk["Bwd"], 180.0, CROUCH_SPEED)]
    samples = []
    for anim, x, y in points:
        s = unreal.BlendSample()
        s.set_editor_property("animation", anim)
        s.set_editor_property("sample_value", unreal.Vector(x, y, 0.0))
        s.set_editor_property("rate_scale", 1.0)
        samples.append(s)
    bs.set_editor_property("sample_data", samples)
    # 샘플을 프로퍼티로 넣으면 런타임 삼각분할(BlendSpaceData)이 다시 계산되지 않아 기본 포즈만 나온다.
    # 파이썬에는 ResampleData가 없고, 블렌드스페이스 에디터가 열릴 때 다시 계산하므로 열었다 닫는다(GUI 에디터에서만 가능)
    editors = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    editors.open_editor_for_assets([bs])
    editors.close_all_editors_for_asset(bs)
    EAL.save_loaded_asset(bs, only_if_is_dirty=False)
    got = [(s.get_editor_property("animation").get_name(), s.get_editor_property("sample_value").x,
            s.get_editor_property("sample_value").y) for s in bs.get_editor_property("sample_data")]
    axes_now = [(str(p.get_editor_property("display_name")), p.get_editor_property("min"), p.get_editor_property("max"))
                for p in bs.get_editor_property("blend_parameters")]
    log(f"블렌드스페이스 {CROUCH_BS} 축={axes_now} 샘플={got}")


def main() -> None:
    import_sources()
    retarget()
    report()
    make_crouch_blendspace()
    log("완료")


main()
