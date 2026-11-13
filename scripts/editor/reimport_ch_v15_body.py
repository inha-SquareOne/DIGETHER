"""SK_Ch_v15_Body를 머리 장식 포함 FBX(SK_Ch_v15_Body_HeadAcc.fbx)로 다시 들여온다(에디터 Python).

FBX는 scripts/art/bind_ch_v15_head_accessories.py로 만든다. 본·몸·옷 메시는 원본 리깅 FBX와 같고
눈·머리카락·수염·콧수염·모자 섹션만 늘어난다(본 55개 동일).
실행(에디터를 닫고):
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4
다음에 apply_ch_v15_materials.py를 실행해 슬롯 머티리얼(머리카락 → M_Hair2)을 지정한다.
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FBX_PATH = os.path.join(PROJECT_DIR, "SourceArt/Model/Character/Character_Model_v1.5/Rig/SK_Ch_v15_Body_HeadAcc.fbx")
ROOT = "/Game/Characters/DwarfV15"
MESH = f"{ROOT}/SK_Ch_v15_Body"
SOCKET = "hand_r_socket"
EXPECTED_SLOTS = {"M_Ch_v15_body", "M_Ch_v15_outfit", "M_Ch_v15_hair"}

EAL = unreal.EditorAssetLibrary


def log(msg: str) -> None:
    unreal.log(f"[reimport_ch_v15_body] {msg}")


def slot_names(mesh) -> set:
    return {str(s.get_editor_property("material_slot_name")) for s in mesh.get_editor_property("materials")}


def main() -> None:
    mesh = EAL.load_asset(MESH)
    if mesh is None:
        raise RuntimeError(f"메시가 없다(retarget_ch_v15.py 먼저): {MESH}")
    if EXPECTED_SLOTS <= slot_names(mesh):
        log("머리 장식 슬롯 있음, 건너뜀")
        return
    if not os.path.isfile(FBX_PATH):
        raise RuntimeError(f"FBX가 없다: {FBX_PATH}")
    skeleton = mesh.skeleton
    had_socket = mesh.find_socket(SOCKET) is not None
    before = {str(s.get_editor_property("material_slot_name")): s.get_editor_property("material_interface")
              for s in mesh.get_editor_property("materials")}

    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("skeleton", skeleton)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX_PATH)
    task.set_editor_property("destination_path", ROOT)
    task.set_editor_property("destination_name", "SK_Ch_v15_Body")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log(f"임포트 결과: {[str(p) for p in task.get_editor_property('imported_object_paths')]}")

    mesh = EAL.load_asset(MESH)
    if mesh.skeleton != skeleton:
        raise RuntimeError(f"스켈레톤이 바뀌었다: {mesh.skeleton.get_path_name()}")
    # 다시 들여오며 비워진 기존 슬롯 머티리얼은 되돌린다(머리카락 슬롯은 apply 스크립트가 지정)
    slots = list(mesh.get_editor_property("materials"))
    for slot in slots:
        name = str(slot.get_editor_property("material_slot_name"))
        if slot.get_editor_property("material_interface") is None and before.get(name) is not None:
            slot.set_editor_property("material_interface", before[name])
    mesh.set_editor_property("materials", slots)
    if had_socket and mesh.find_socket(SOCKET) is None:
        raise RuntimeError(f"{SOCKET}이 사라졌다. swap_player_to_ch_v15.py로 다시 만든다")
    if not EAL.save_loaded_asset(mesh, only_if_is_dirty=False):
        raise RuntimeError(f"저장 실패(에디터가 열려 있는지 확인): {MESH}")
    log(f"슬롯 {sorted(slot_names(mesh))}, 소켓 {'유지' if mesh.find_socket(SOCKET) else '없음'}, "
        f"바운드 {mesh.get_imported_bounds().box_extent}")


main()
