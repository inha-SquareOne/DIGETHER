"""곡괭이 아이템 정의와 줍기·버리기 입력 에셋을 만들고 연결한다(에디터 Python).

실행(에디터를 닫고):
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4
에디터 안에서는 Tools > Execute Python Script로 실행해도 된다. 이미 있는 에셋은 값을 덮어쓰지 않는다.

- /Game/Data/Items/DA_Item_Pickaxe (UDigetherItemDefinition): 장비, 정산 0, 무게 3kg(시험값, 장비 목록 11/18 확정 전)
  WorldMesh는 옮긴 Fab 곡괭이 메시(임시용)로 지정한다. 메시가 없으면 월드 아이템이 기본 상자로 보인다.
- Fab 임시 곡괭이(/Game/Fab/Megascans/3D/Pick_Axe_wlkkdfz)를 /Game/Developers/taewoong/Pickaxe로 옮긴다
- /Game/Game/Input/Player/IA_Interact (E), IA_Drop (G): 불리언 입력 액션
- DA_PlayerInput의 InteractAction·DropAction에 연결(비어 있을 때만)
"""

import unreal

ITEM_DIR = "/Game/Data/Items"
ITEM_NAME = "DA_Item_Pickaxe"
INPUT_DIR = "/Game/Game/Input/Player"
INPUT_DATA = f"{INPUT_DIR}/DA_PlayerInput"
EAL = unreal.EditorAssetLibrary


def log(msg: str) -> None:
    unreal.log(f"[create_pickaxe_item_and_input] {msg}")


def make_tag(name: str) -> unreal.GameplayTag:
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    if str(tag.export_text()).find(name) < 0:
        raise RuntimeError(f"태그를 만들지 못했습니다: {name}")
    return tag


def create_asset(directory: str, name: str, asset_class, factory):
    path = f"{directory}/{name}"
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path), False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, asset_class, factory)
    if asset is None:
        raise RuntimeError(f"에셋을 만들지 못했습니다: {path}")
    return asset, True


FAB_DIR = "/Game/Fab/Megascans/3D/Pick_Axe_wlkkdfz/Medium/wlkkdfz_tier_2"
PICKAXE_DEST = "/Game/Developers/taewoong/Pickaxe"
# Fab 임시 곡괭이를 개발자 폴더로 옮길 에셋(원래 하위 폴더 -> 새 하위 폴더)
FAB_MOVES = (
    ("StaticMeshes/wlkkdfz_tier_2", "Meshes/wlkkdfz_tier_2"),
    ("Materials/MI_wlkkdfz", "Materials/MI_wlkkdfz"),
    ("Textures/T_wlkkdfz_2K_B", "Textures/T_wlkkdfz_2K_B"),
    ("Textures/T_wlkkdfz_2K_N", "Textures/T_wlkkdfz_2K_N"),
    ("Textures/T_wlkkdfz_2K_ORM", "Textures/T_wlkkdfz_2K_ORM"),
)
PICKAXE_MESH = f"{PICKAXE_DEST}/Meshes/wlkkdfz_tier_2"


def move_fab_pickaxe() -> None:
    """Fab 곡괭이를 Developers/taewoong/Pickaxe로 옮기고 리다이렉터를 정리한다. 이미 옮겼으면 건너뛴다."""
    moved = False
    for old, new in FAB_MOVES:
        old_path, new_path = f"{FAB_DIR}/{old}", f"{PICKAXE_DEST}/{new}"
        if EAL.does_asset_exist(new_path):
            continue
        if not EAL.does_asset_exist(old_path):
            raise RuntimeError(f"곡괭이 에셋이 없습니다: {old_path}")
        if not EAL.rename_asset(old_path, new_path):
            raise RuntimeError(f"옮기지 못했습니다: {old_path} -> {new_path}")
        moved = True
    if moved:
        redirectors = unreal.AssetToolsHelpers.get_asset_tools()
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        found = registry.get_assets_by_path(FAB_DIR, recursive=True)
        to_fix = [a.get_asset() for a in found if str(a.asset_class_path.asset_name) == "ObjectRedirector"]
        if to_fix:
            redirectors.fix_up_redirectors(to_fix)
        EAL.save_directory(PICKAXE_DEST, only_if_is_dirty=False, recursive=True)
    log(f"Fab 곡괭이 이동: {'완료' if moved else '이미 옮겨져 있음'}")


def create_pickaxe() -> None:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DigetherItemDefinition)
    item, created = create_asset(ITEM_DIR, ITEM_NAME, unreal.DigetherItemDefinition, factory)
    mesh = unreal.load_asset(PICKAXE_MESH)
    if mesh is not None and item.get_editor_property("world_mesh") is None:
        item.set_editor_property("world_mesh", mesh)
        created = True
    if created:
        item.set_editor_property("display_name", unreal.Text("곡괭이"))
        item.set_editor_property("item_type", make_tag("Item.Type.Equipment"))
        item.set_editor_property("value_credits", 0)
        item.set_editor_property("weight_kg", 3.0)
        EAL.save_asset(f"{ITEM_DIR}/{ITEM_NAME}")
    log(f"{ITEM_NAME}: {'생성' if created else '이미 있음(유지)'}")


def create_input_action(name: str):
    factory = unreal.InputAction_Factory() if hasattr(unreal, "InputAction_Factory") else None
    action, created = create_asset(INPUT_DIR, name, unreal.InputAction, factory)
    if created:
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        EAL.save_asset(f"{INPUT_DIR}/{name}")
    log(f"{name}: {'생성' if created else '이미 있음(유지)'}")
    return action


def connect_input() -> None:
    data = unreal.load_asset(INPUT_DATA)
    if data is None:
        raise RuntimeError(f"입력 데이터를 찾지 못했습니다: {INPUT_DATA}")
    for prop, name in (("interact_action", "IA_Interact"), ("drop_action", "IA_Drop")):
        action = create_input_action(name)
        if data.get_editor_property(prop) is None:
            data.set_editor_property(prop, action)
            log(f"DA_PlayerInput.{prop} <- {name}")
    EAL.save_asset(INPUT_DATA)


move_fab_pickaxe()
create_pickaxe()
connect_input()
log("완료")
