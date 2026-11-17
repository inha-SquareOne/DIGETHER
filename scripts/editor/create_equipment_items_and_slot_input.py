"""삽·곡괭이를 소지품 장비 아이템으로 연결하고 1~4 슬롯 선택 입력을 만든다(에디터 Python).

실행(에디터를 닫고, create_pickaxe_item_and_input.py를 먼저 실행한 뒤):
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4
이미 값이 있는 항목은 덮어쓰지 않는다.

- /Game/Data/Items/DA_Item_Shovel: 장비, 정산 0, 무게 2kg(이전 시험 지급값), WorldMesh=SM_Shovel, EquipmentClass=BP_Weapon_Shovel
- /Game/Game/Weapon/Pickaxe/BP_Weapon_Pickaxe: BP_Weapon_Shovel 복제 후 메시만 곡괭이로 교체
  (공격 몽타주·판정 값·잡는 위치는 삽 것을 그대로 쓰는 임시 설정, 곡괭이 전용 값은 장비 목록 확정 후)
- DA_Item_Pickaxe.EquipmentClass=BP_Weapon_Pickaxe
- /Game/Game/Input/Player/IA_SelectSlot1~4 (불리언), DA_PlayerInput.SelectSlotActions에 순서대로 연결
"""

import unreal

EAL = unreal.EditorAssetLibrary
ITEM_DIR = "/Game/Data/Items"
INPUT_DIR = "/Game/Game/Input/Player"
INPUT_DATA = f"{INPUT_DIR}/DA_PlayerInput"
SHOVEL_BP = "/Game/Game/Weapon/Shovel/BP_Weapon_Shovel"
SHOVEL_MESH = "/Game/Game/Weapon/Shovel/SM_Shovel"
PICKAXE_BP_DIR = "/Game/Game/Weapon/Pickaxe"
PICKAXE_BP = f"{PICKAXE_BP_DIR}/BP_Weapon_Pickaxe"
PICKAXE_MESH = "/Game/Developers/taewoong/Pickaxe/Meshes/wlkkdfz_tier_2"
PICKAXE_ITEM = f"{ITEM_DIR}/DA_Item_Pickaxe"


def log(msg: str) -> None:
    unreal.log(f"[create_equipment_items_and_slot_input] {msg}")


def require(path: str):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"에셋이 없습니다: {path}")
    return asset


def make_tag(name: str) -> unreal.GameplayTag:
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    if str(tag.export_text()).find(name) < 0:
        raise RuntimeError(f"태그를 만들지 못했습니다: {name}")
    return tag


def create_shovel_item() -> None:
    path = f"{ITEM_DIR}/DA_Item_Shovel"
    if EAL.does_asset_exist(path):
        item = unreal.load_asset(path)
        created = False
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.DigetherItemDefinition)
        item = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_Item_Shovel", ITEM_DIR, unreal.DigetherItemDefinition, factory)
        if item is None:
            raise RuntimeError(f"에셋을 만들지 못했습니다: {path}")
        item.set_editor_property("display_name", unreal.Text("삽"))
        item.set_editor_property("item_type", make_tag("Item.Type.Equipment"))
        item.set_editor_property("value_credits", 0)
        item.set_editor_property("weight_kg", 2.0)
        created = True
    changed = created
    if item.get_editor_property("world_mesh") is None:
        item.set_editor_property("world_mesh", require(SHOVEL_MESH))
        changed = True
    if item.get_editor_property("equipment_class") is None:
        item.set_editor_property("equipment_class", require(SHOVEL_BP).generated_class())
        changed = True
    if changed:
        EAL.save_asset(path)
    log(f"DA_Item_Shovel: {'생성' if created else ('갱신' if changed else '이미 있음(유지)')}")


def create_pickaxe_weapon():
    if not EAL.does_asset_exist(PICKAXE_BP):
        if EAL.duplicate_asset(SHOVEL_BP, PICKAXE_BP) is None:
            raise RuntimeError(f"복제하지 못했습니다: {SHOVEL_BP} -> {PICKAXE_BP}")
        bp = require(PICKAXE_BP)
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.get_editor_property("weapon_mesh").set_editor_property("static_mesh", require(PICKAXE_MESH))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        EAL.save_asset(PICKAXE_BP)
        log("BP_Weapon_Pickaxe: 생성(삽 BP 복제, 메시 교체)")
    else:
        log("BP_Weapon_Pickaxe: 이미 있음(유지)")
    return require(PICKAXE_BP)


def connect_pickaxe_item(pickaxe_bp) -> None:
    item = require(PICKAXE_ITEM)
    if item.get_editor_property("equipment_class") is None:
        item.set_editor_property("equipment_class", pickaxe_bp.generated_class())
        EAL.save_asset(PICKAXE_ITEM)
        log("DA_Item_Pickaxe.EquipmentClass <- BP_Weapon_Pickaxe")
    else:
        log("DA_Item_Pickaxe.EquipmentClass 이미 지정됨, 유지")


def create_slot_inputs() -> None:
    actions = []
    for number in range(1, 5):
        name = f"IA_SelectSlot{number}"
        path = f"{INPUT_DIR}/{name}"
        if EAL.does_asset_exist(path):
            action = unreal.load_asset(path)
        else:
            factory = unreal.InputAction_Factory() if hasattr(unreal, "InputAction_Factory") else None
            action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, INPUT_DIR, unreal.InputAction, factory)
            if action is None:
                raise RuntimeError(f"에셋을 만들지 못했습니다: {path}")
            action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            EAL.save_asset(path)
            log(f"{name}: 생성")
        actions.append(action)
    data = require(INPUT_DATA)
    if len(data.get_editor_property("select_slot_actions")) == 0:
        data.set_editor_property("select_slot_actions", actions)
        EAL.save_asset(INPUT_DATA)
        log("DA_PlayerInput.SelectSlotActions <- IA_SelectSlot1~4")
    else:
        log("DA_PlayerInput.SelectSlotActions 이미 지정됨, 유지")


create_shovel_item()
connect_pickaxe_item(create_pickaxe_weapon())
create_slot_inputs()
log("완료")
