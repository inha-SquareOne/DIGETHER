"""캐릭터 모델 v1.5(SK_Ch_v15_Body)를 들여오고 마네킹 애니메이션을 리타겟한다(에디터 Python).

실행(에디터를 닫고):
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4
에디터 안에서는 Tools > Execute Python Script로 실행해도 된다.

만드는 것(/Game/Characters/DwarfV15):
- SK_Ch_v15_Body(+Skeleton, PhysicsAsset, 머티리얼): SourceArt/Model/Character/Character_Model_v1.5/Rig/SK_Ch_v15_Body.fbx
- T_Ch_BC_body/hair/outfit 텍스처(머티리얼 연결은 하지 않음)
- Retarget/IK_Quinn_Source, IK_Ch_v15_Body, RTG_Quinn_To_Ch_v15
- Anims/: ABP_Unarmed와 참조 애니메이션 + MM_Attack_01을 이름 뒤 V15로 리타겟

기존 드워프(SK_Ch_Dwarf_Miner_UE3) 에셋과 플레이어 기본 메시는 건드리지 않는다.
이미 있는 에셋은 다시 만들지 않는다. 다시 만들려면 지우고 실행한다.
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART_DIR = os.path.join(PROJECT_DIR, "SourceArt/Model/Character/Character_Model_v1.5")
FBX_PATH = os.path.join(ART_DIR, "Rig/SK_Ch_v15_Body.fbx")
TEXTURES = ["T_Ch_BC_body.png", "T_Ch_BC_hair.png", "T_Ch_BC_outfit.png"]

ROOT = "/Game/Characters/DwarfV15"
RETARGET_DIR = f"{ROOT}/Retarget"
ANIM_DIR = f"{ROOT}/Anims"
TARGET_MESH = f"{ROOT}/SK_Ch_v15_Body"

SOURCE_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
SOURCE_ANIMS = [
    "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed",
    "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01",
]
SUFFIX = "V15"

SOURCE_RIG = f"{RETARGET_DIR}/IK_Quinn_Source"
TARGET_RIG = f"{RETARGET_DIR}/IK_Ch_v15_Body"
RETARGETER = f"{RETARGET_DIR}/RTG_Quinn_To_Ch_v15"

EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SIDE = unreal.RetargetSourceOrTarget


def log(msg: str) -> None:
    unreal.log(f"[retarget_ch_v15] {msg}")


def load(path: str):
    asset = EAL.load_asset(path)
    if asset is None:
        raise RuntimeError(f"에셋을 찾을 수 없다: {path}")
    return asset


def import_file(path: str, dest: str, options=None) -> list:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", False)
    if options is not None:
        task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    return [str(p) for p in task.get_editor_property("imported_object_paths")]


def import_mesh() -> unreal.SkeletalMesh:
    if EAL.does_asset_exist(TARGET_MESH):
        log(f"메시 있음, 건너뜀: {TARGET_MESH}")
        return load(TARGET_MESH)
    if not os.path.isfile(FBX_PATH):
        raise RuntimeError(f"FBX가 없다(SVN 업데이트 필요): {FBX_PATH}")
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", True)
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
    paths = import_file(FBX_PATH, ROOT, ui)
    log(f"FBX 임포트 결과: {paths}")
    meshes = [p for p in paths if isinstance(EAL.load_asset(p), unreal.SkeletalMesh)]
    if not meshes:
        raise RuntimeError("스켈레탈 메시가 만들어지지 않았다")
    created = meshes[0].split(".")[0]
    if created != TARGET_MESH and not EAL.rename_asset(created, TARGET_MESH):
        raise RuntimeError(f"이동 실패: {created} -> {TARGET_MESH}")
    EAL.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
    return load(TARGET_MESH)


def import_textures() -> None:
    for name in TEXTURES:
        target = f"{ROOT}/{os.path.splitext(name)[0]}"
        if EAL.does_asset_exist(target):
            continue
        src = os.path.join(ART_DIR, "PNG", name)
        if os.path.isfile(src):
            log(f"텍스처 임포트: {import_file(src, ROOT)}")


def make_ik_rig(path: str, mesh: unreal.SkeletalMesh) -> unreal.IKRigDefinition:
    if EAL.does_asset_exist(path):
        log(f"IK Rig 있음, 건너뜀: {path}")
        return load(path)
    folder, name = path.rsplit("/", 1)
    rig = TOOLS.create_asset(name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(mesh)
    if not ctrl.apply_auto_generated_retarget_definition():
        raise RuntimeError(f"리타겟 정의 자동 생성 실패: {path}")
    ctrl.apply_auto_fbik()
    chains = []
    for c in ctrl.get_retarget_chains():
        n = c.get_editor_property("chain_name")
        chains.append(f"{n}({ctrl.get_retarget_chain_start_bone(n)}->{ctrl.get_retarget_chain_end_bone(n)})")
    log(f"{name} 루트={ctrl.get_retarget_root()} 체인 {len(chains)}개: {chains}")
    EAL.save_loaded_asset(rig, only_if_is_dirty=False)
    return rig


def make_retargeter(src_rig, tgt_rig, src_mesh, tgt_mesh) -> unreal.IKRetargeter:
    if EAL.does_asset_exist(RETARGETER):
        log(f"리타게터 있음, 건너뜀: {RETARGETER}")
        return load(RETARGETER)
    folder, name = RETARGETER.rsplit("/", 1)
    rtg = TOOLS.create_asset(name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.set_ik_rig(SIDE.SOURCE, src_rig)
    ctrl.set_ik_rig(SIDE.TARGET, tgt_rig)
    ctrl.set_preview_mesh(SIDE.SOURCE, src_mesh)
    ctrl.set_preview_mesh(SIDE.TARGET, tgt_mesh)
    if ctrl.get_num_retarget_ops() == 0:
        ctrl.add_default_ops()
    ctrl.assign_ik_rig_to_all_ops(SIDE.SOURCE, src_rig)
    ctrl.assign_ik_rig_to_all_ops(SIDE.TARGET, tgt_rig)
    ctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    # 소스(Quinn A포즈)와 대상의 팔 각도가 달라 대상 리타겟 포즈를 소스에 맞춘다
    ctrl.auto_align_all_bones(SIDE.TARGET)
    ops = [str(ctrl.get_op_name(i)) for i in range(ctrl.get_num_retarget_ops())]
    log(f"리타게터 오퍼레이션: {ops}")
    for chain in tgt_rig_chains(tgt_rig):
        log(f"  체인 매핑 {chain} <- {ctrl.get_source_chain(chain)}")
    EAL.save_loaded_asset(rtg, only_if_is_dirty=False)
    return rtg


def tgt_rig_chains(rig) -> list:
    return [str(c.get_editor_property("chain_name")) for c in unreal.IKRigController.get_controller(rig).get_retarget_chains()]


def retarget_anims(src_mesh, tgt_mesh, rtg) -> list:
    todo = [p for p in SOURCE_ANIMS if not EAL.does_asset_exist(f"{ANIM_DIR}/{p.rsplit('/', 1)[1]}{SUFFIX}")]
    if not todo:
        log("리타겟 애니메이션 모두 있음, 건너뜀")
        return []
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", [EAL.find_asset_data(p) for p in todo])
    inputs.set_editor_property("source_mesh", src_mesh)
    inputs.set_editor_property("target_mesh", tgt_mesh)
    inputs.set_editor_property("ik_retarget_asset", rtg)
    inputs.set_editor_property("suffix", SUFFIX)
    inputs.set_editor_property("include_referenced_assets", True)
    inputs.set_editor_property("overwrite_existing_files", False)
    inputs.set_editor_property("use_source_path", False)
    inputs.set_editor_property("target_path", ANIM_DIR)
    results = [str(d.package_name) for d in unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)]
    log(f"리타겟 생성 {len(results)}개: {results}")
    EAL.save_directory(ANIM_DIR, only_if_is_dirty=False, recursive=True)
    return results


def component_z(seq, bone: str, time: float, parents: dict) -> float:
    """애니메이션의 한 시점에서 본의 컴포넌트 공간 위치(cm)를 계산한다."""
    t = unreal.Transform()
    b = bone
    chain = []
    while b:
        chain.append(b)
        b = parents.get(b)
    for name in reversed(chain):
        local = unreal.AnimationLibrary.get_bone_pose_for_time(seq, name, time, False)
        t = unreal.MathLibrary.compose_transforms(local, t)
    return t.translation


def report_poses() -> None:
    """리타겟 결과의 발·손·머리 높이를 남겨 자세 이상(바닥 관통, 팔 뒤집힘)을 확인한다."""
    order = [("pelvis", "root"), ("spine_01", "pelvis"), ("spine_02", "spine_01"), ("spine_03", "spine_02"),
             ("spine_04", "spine_03"), ("spine_05", "spine_04"), ("neck_01", "spine_05"), ("head", "neck_01"),
             ("clavicle_r", "spine_05"), ("upperarm_r", "clavicle_r"), ("lowerarm_r", "upperarm_r"),
             ("hand_r", "lowerarm_r"), ("thigh_l", "pelvis"), ("calf_l", "thigh_l"), ("foot_l", "calf_l"),
             ("ball_l", "foot_l")]
    parents = dict(order)
    parents["root"] = None
    for path in EAL.list_assets(ANIM_DIR, recursive=False):
        seq = EAL.load_asset(path)
        if not isinstance(seq, unreal.AnimSequence):
            continue
        length = seq.get_play_length()
        parts = []
        for time in (0.0, length * 0.5):
            vals = {b: component_z(seq, b, time, parents) for b in ("ball_l", "hand_r", "head")}
            parts.append(" ".join(f"{b}=({v.x:.0f},{v.y:.0f},{v.z:.0f})" for b, v in vals.items()))
        log(f"  자세 {seq.get_name()} 길이 {length:.2f}s | t0 {parts[0]} | 중간 {parts[1]}")


def report_abp_parents() -> None:
    for path in ["/Game/Characters/Dwarf/Anims/ABP_UnarmedDG", f"{ANIM_DIR}/ABP_Unarmed{SUFFIX}"]:
        if not EAL.does_asset_exist(path):
            log(f"{path} 없음")
            continue
        data = EAL.find_asset_data(path)
        bp = data.get_asset()
        log(f"{path} 부모 클래스={data.get_tag_value('ParentClass')} 스켈레톤={bp.get_editor_property('target_skeleton').get_name()}")


def main() -> None:
    src_mesh = load(SOURCE_MESH)
    tgt_mesh = import_mesh()
    log(f"대상 스켈레톤={tgt_mesh.skeleton.get_path_name()}")
    import_textures()
    src_rig = make_ik_rig(SOURCE_RIG, src_mesh)
    tgt_rig = make_ik_rig(TARGET_RIG, tgt_mesh)
    rtg = make_retargeter(src_rig, tgt_rig, src_mesh, tgt_mesh)
    retarget_anims(src_mesh, tgt_mesh, rtg)
    report_poses()
    report_abp_parents()
    log("완료")


main()
