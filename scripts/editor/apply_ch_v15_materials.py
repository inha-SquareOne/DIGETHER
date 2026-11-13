"""캐릭터 v1.5 메시(SK_Ch_v15_Body)에 이전 드워프 머티리얼 구성을 적용한다(에디터 Python).

실행(에디터를 닫고):
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4

- 몸 슬롯: 이전 드워프의 M_Body2(T_Ch_BC_body → BaseColor)를 그대로 쓴다. v1.0과 v1.5 몸 텍스처 PNG는 같은 파일이다.
- 옷 슬롯: 이전 드워프 옷(Material__43)은 텍스처 없는 FBX 기본 머티리얼이었다. M_Body2와 같은 구조로
  M_Ch_Outfit을 만들고 v1.5 옷 텍스처(T_Ch_BC_outfit)를 연결한다.
- 머리카락·수염·콧수염 슬롯(M_Ch_v15_hair): 이전 드워프의 M_Hair2(T_Ch_BC_hair, v1.0·v1.5 같은 파일).
  이 슬롯은 reimport_ch_v15_body.py로 머리 장식 포함 FBX를 들여온 뒤 생긴다. 모자는 옷, 눈은 몸 슬롯을 쓴다.
이미 적용돼 있으면 건너뛴다.
"""

import unreal

MESH = "/Game/Characters/DwarfV15/SK_Ch_v15_Body"
BODY_MATERIAL = "/Game/Characters/Dwarf/M_Body2"
OUTFIT_MATERIAL = "/Game/Characters/DwarfV15/M_Ch_Outfit"
OUTFIT_TEXTURE = "/Game/Characters/DwarfV15/T_Ch_BC_outfit"
HAIR_MATERIAL = "/Game/Characters/Dwarf/M_Hair2"
SLOT_MATERIALS = {"M_Ch_v15_body": BODY_MATERIAL, "M_Ch_v15_outfit": OUTFIT_MATERIAL, "M_Ch_v15_hair": HAIR_MATERIAL}

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def log(msg: str) -> None:
    unreal.log(f"[apply_ch_v15_materials] {msg}")


def load(path: str):
    asset = EAL.load_asset(path)
    if asset is None:
        raise RuntimeError(f"에셋을 찾을 수 없다: {path}")
    return asset


def make_outfit_material() -> None:
    if EAL.does_asset_exist(OUTFIT_MATERIAL):
        log(f"옷 머티리얼 있음, 건너뜀: {OUTFIT_MATERIAL}")
        return
    mat = EAL.duplicate_asset(BODY_MATERIAL, OUTFIT_MATERIAL)
    if mat is None:
        raise RuntimeError(f"복제 실패: {BODY_MATERIAL} -> {OUTFIT_MATERIAL}")
    node = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    if not isinstance(node, unreal.MaterialExpressionTextureBase):
        raise RuntimeError(f"BaseColor 입력이 텍스처 샘플이 아니다: {node}")
    node.set_editor_property("texture", load(OUTFIT_TEXTURE))
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, only_if_is_dirty=False)
    log(f"옷 머티리얼 생성: {OUTFIT_MATERIAL} (BaseColor={OUTFIT_TEXTURE})")


def apply_to_mesh() -> None:
    mesh = load(MESH)
    slots = list(mesh.get_editor_property("materials"))
    changed = False
    for slot in slots:
        name = str(slot.get_editor_property("material_slot_name"))
        want = SLOT_MATERIALS.get(name)
        if want is None:
            log(f"알 수 없는 슬롯, 유지: {name}")
            continue
        current = slot.get_editor_property("material_interface")
        if current is not None and current.get_path_name().split(".")[0] == want:
            continue
        slot.set_editor_property("material_interface", load(want))
        changed = True
        log(f"슬롯 {name} -> {want}")
    if changed:
        mesh.set_editor_property("materials", slots)
        EAL.save_loaded_asset(mesh, only_if_is_dirty=False)
    else:
        log("메시 슬롯 이미 적용됨")


def main() -> None:
    make_outfit_material()
    apply_to_mesh()
    log("완료")


main()
