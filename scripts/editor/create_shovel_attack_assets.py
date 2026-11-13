"""삽 공격 애니메이션 에셋을 만들고 BP_Weapon_Shovel에 연결한다(에디터 Python).

실행(C++ 빌드 후, 에디터를 닫고):
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4
에디터 안에서는 Tools > Execute Python Script로 실행해도 된다.

플레이어 몸(캐릭터 v1.5) 애니메이션과 같은 구조로 만든다:
- 마네킹 애니메이션을 RTG_Quinn_To_Ch_v15로 리타겟, 이름 뒤에 V15, 위치 /Game/Characters/DwarfV15/Anims(retarget_ch_v15.py와 같은 규칙)
- 몽타주는 장비 폴더(/Game/Game/Weapon/Shovel)에 두고 슬롯은 DefaultSlot(ABP의 Slot 노드 이름, 그룹 이름 없이)
- 공격 애니메이션의 루트 모션을 끄고 루트를 고정한다(원본은 앞으로 돌진하는 모션)
- 몽타주에 MeleeHitWindow 노티파이 구간(히트박스가 존재하는 시간)을 넣는다. 시작·길이는 임시값이며 몽타주 타임라인에서 끌어서 조정한다
- BP_Weapon_Shovel의 AttackMontage·MeleeHit(판정 값)을 채운다. 판정 값은 모두 시험값이다
  (피해 12.5는 사용자 결정 3의 '기본 공격 12.5 가정', 장비별 피해량 D2·D3은 미정)

이미 있는 애니메이션·몽타주는 다시 만들지 않는다(사람이 조정한 노티파이 위치 보존). 다시 만들려면 지우고 실행한다.
BP의 판정 값도 이미 0이 아닌 값이 있으면 덮어쓰지 않는다.
"""

import unreal

# 원본 동작: 삽 전용 동작이 오기 전까지 쓰는 마네킹 공격 모션(교체 시 이 값만 바꾼다)
SOURCE_ANIM = "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01"
SOURCE_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
TARGET_MESH = "/Game/Characters/DwarfV15/SK_Ch_v15_Body"
RETARGETER = "/Game/Characters/DwarfV15/Retarget/RTG_Quinn_To_Ch_v15"
DWARF_ANIM_DIR = "/Game/Characters/DwarfV15/Anims"
DWARF_SUFFIX = "V15"

SHOVEL_DIR = "/Game/Game/Weapon/Shovel"
MONTAGE_NAME = "AM_Shovel_Attack"
SHOVEL_BP = "/Game/Game/Weapon/Shovel/BP_Weapon_Shovel"
# ABP_UnarmedV15(ABP_UnarmedDG에서 리타겟)의 몽타주 Slot 노드는 DefaultSlot 하나다.
# 몽타주 트랙의 슬롯 이름은 그룹 없이 슬롯 이름만 쓴다(에디터 표시 'DefaultGroup.DefaultSlot'을 그대로 넣으면 노드와 안 맞는다)
SLOT_NAME = "DefaultSlot"

# 히트박스 구간(몽타주 길이 대비 비율), 임시값
HIT_WINDOW_TRACK = "Hit"
HIT_WINDOW_START_RATIO = 0.30
HIT_WINDOW_LENGTH_RATIO = 0.20

# 판정 시험값
MELEE_VALUES = {
    "damage": 12.5,
    "forward_offset_cm": 0.0,
    "range_cm": 150.0,
    "half_width_cm": 35.0,
    "half_height_cm": 35.0,
}

EAL = unreal.EditorAssetLibrary


def log(msg: str) -> None:
    unreal.log(f"[create_shovel_attack_assets] {msg}")


def load(path: str):
    asset = EAL.load_asset(path)
    if asset is None:
        raise RuntimeError(f"에셋을 찾을 수 없다: {path}")
    return asset


def retarget_attack_anim() -> unreal.AnimSequence:
    source_name = SOURCE_ANIM.rsplit("/", 1)[1]
    target_path = f"{DWARF_ANIM_DIR}/{source_name}{DWARF_SUFFIX}"
    if EAL.does_asset_exist(target_path):
        log(f"리타겟 애니메이션 있음, 건너뜀: {target_path}")
        return load(target_path)

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", [EAL.find_asset_data(SOURCE_ANIM)])
    inputs.set_editor_property("source_mesh", load(SOURCE_MESH))
    inputs.set_editor_property("target_mesh", load(TARGET_MESH))
    inputs.set_editor_property("ik_retarget_asset", load(RETARGETER))
    inputs.set_editor_property("suffix", DWARF_SUFFIX)
    inputs.set_editor_property("include_referenced_assets", False)
    inputs.set_editor_property("overwrite_existing_files", False)
    inputs.set_editor_property("use_source_path", False)
    inputs.set_editor_property("target_path", DWARF_ANIM_DIR)
    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not results:
        raise RuntimeError("리타겟 결과가 없다")
    created_path = None
    for data in results:
        path = str(data.package_name)
        if path.endswith(source_name + DWARF_SUFFIX):
            created_path = path
    if created_path is None:
        raise RuntimeError(f"리타겟 결과에서 {source_name}{DWARF_SUFFIX}를 찾지 못했다: {[str(d.package_name) for d in results]}")
    if created_path != target_path:
        if not EAL.rename_asset(created_path, target_path):
            raise RuntimeError(f"이동 실패: {created_path} -> {target_path}")
    EAL.save_asset(target_path, only_if_is_dirty=False)
    log(f"리타겟 생성: {target_path}")
    return load(target_path)


def lock_root_motion(sequence: unreal.AnimSequence) -> None:
    """원본 MM_Attack_01은 루트가 약 1.2m 앞으로 나가는 돌진 모션이다.
    삽 공격은 제자리 공격이므로 루트 모션을 끄고 루트를 기준 자세에 고정한다(캐릭터가 밀려나지 않게)."""
    if not sequence.get_editor_property("enable_root_motion") and sequence.get_editor_property("force_root_lock"):
        return
    sequence.set_editor_property("enable_root_motion", False)
    sequence.set_editor_property("force_root_lock", True)
    EAL.save_loaded_asset(sequence, only_if_is_dirty=False)
    log(f"루트 모션 끔·루트 고정: {sequence.get_path_name()}")


def fix_slot(montage: unreal.AnimMontage) -> None:
    """이미 있는 몽타주의 슬롯만 맞춘다(사람이 조정한 노티파이는 건드리지 않는다)."""
    tracks = montage.get_editor_property("slot_anim_tracks")
    if not tracks or str(tracks[0].get_editor_property("slot_name")) == SLOT_NAME:
        return
    old = tracks[0].get_editor_property("slot_name")
    track = tracks[0]
    track.set_editor_property("slot_name", SLOT_NAME)
    tracks[0] = track
    montage.set_editor_property("slot_anim_tracks", tracks)
    EAL.save_loaded_asset(montage, only_if_is_dirty=False)
    log(f"몽타주 슬롯 변경: {old} -> {SLOT_NAME}")


def create_montage(sequence: unreal.AnimSequence) -> unreal.AnimMontage:
    montage_path = f"{SHOVEL_DIR}/{MONTAGE_NAME}"
    if EAL.does_asset_exist(montage_path):
        montage = load(montage_path)
        fix_slot(montage)
        log(f"몽타주 있음, 노티파이 유지: {montage_path}")
        return montage

    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", sequence)
    factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MONTAGE_NAME, SHOVEL_DIR, unreal.AnimMontage, factory)
    if montage is None:
        raise RuntimeError("몽타주 생성 실패")

    tracks = montage.get_editor_property("slot_anim_tracks")
    if tracks:
        track = tracks[0]
        track.set_editor_property("slot_name", SLOT_NAME)
        tracks[0] = track
        montage.set_editor_property("slot_anim_tracks", tracks)

    length = montage.get_play_length()
    start = length * HIT_WINDOW_START_RATIO
    duration = length * HIT_WINDOW_LENGTH_RATIO
    lib = unreal.AnimationLibrary
    if not lib.is_valid_anim_notify_track_name(montage, HIT_WINDOW_TRACK):
        lib.add_animation_notify_track(montage, HIT_WINDOW_TRACK)
    lib.add_animation_notify_state_event(
        montage, HIT_WINDOW_TRACK, start, duration, unreal.DigetherAnimNotifyState_MeleeHitWindow)

    EAL.save_loaded_asset(montage, only_if_is_dirty=False)
    log(f"몽타주 생성: {montage_path} (길이 {length:.2f}s, 판정 {start:.2f}~{start + duration:.2f}s, 슬롯 {SLOT_NAME})")
    return montage


def connect_shovel(montage: unreal.AnimMontage) -> None:
    bp = load(SHOVEL_BP)
    cdo = unreal.get_default_object(bp.generated_class())
    changed = False
    if cdo.get_editor_property("attack_montage") is None:
        cdo.set_editor_property("attack_montage", montage)
        changed = True
        log("BP_Weapon_Shovel.AttackMontage 연결")
    else:
        log("BP_Weapon_Shovel.AttackMontage 이미 지정됨, 유지")

    melee = cdo.get_editor_property("melee_hit")
    if melee.get_editor_property("damage") <= 0.0:
        for key, value in MELEE_VALUES.items():
            melee.set_editor_property(key, value)
        cdo.set_editor_property("melee_hit", melee)
        changed = True
        log(f"BP_Weapon_Shovel.MeleeHit 시험값 입력: {MELEE_VALUES}")
    else:
        log("BP_Weapon_Shovel.MeleeHit 이미 값이 있음, 유지")

    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        EAL.save_loaded_asset(bp, only_if_is_dirty=False)


def main() -> None:
    sequence = retarget_attack_anim()
    lock_root_motion(sequence)
    montage = create_montage(sequence)
    connect_shovel(montage)
    log(f"완료. 슬롯 {SLOT_NAME}")


main()
