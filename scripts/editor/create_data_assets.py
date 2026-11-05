"""정의 데이터 에셋을 만들고 프로토타입 BP에 연결한다(에디터 Python, 코드 구조 계획 P1).

실행(에디터를 닫고):
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4

- /Game/Data/Tuning/DA_Tuning_Prototype (UDigetherTuningProfile)
- /Game/Data/Items/DA_Item_PrototypeCargo (UDigetherItemDefinition)
- BP_PrototypeGameMode.TuningProfile, BP_PrototypePickup.ItemDefinition 연결

값은 P1 이전의 BP_PrototypeGameMode 기본값(RunSeconds만 BP에서 600으로 덮어씀), Rules.h의 100%/50%,
ADigetherPrototypePickup 기본값(40원·5kg·한 손)이다. 모두 시험 픽스처이며 최종 기획 값이 아니다.
이미 있는 DA는 값을 덮어쓰지 않는다(기획자가 고친 값을 보존). 다시 만들려면 DA를 지우고 실행한다.
"""

import unreal

TUNING_DIR = "/Game/Data/Tuning"
TUNING_NAME = "DA_Tuning_Prototype"
ITEM_DIR = "/Game/Data/Items"
CARGO_NAME = "DA_Item_PrototypeCargo"
GAME_MODE_BP = "/Game/Prototype/Blueprints/BP_PrototypeGameMode"
PICKUP_BP = "/Game/Prototype/Blueprints/BP_PrototypePickup"

TUNING_VALUES = {
    "profile_version": 1,
    "initial_quota_credits": 100,
    "run_seconds": 600.0,
    "interaction_distance_cm": 350.0,
    "elevator_capacity_kg": 40.0,
    "elevator_trip_limit": 2,
    "ascent_penalty_seconds": 10.0,
    "elevator_travel_seconds": 4.0,
    "deposit_interval_seconds": 0.25,
    "emergency_weight_limit_kg": 5.0,
    "death_loss_percent": 100,
    "corpse_refund_percent": 50,
}


def make_tag(name: str) -> unreal.GameplayTag:
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    if str(tag.export_text()).find(name) < 0:
        raise RuntimeError(f"태그를 만들지 못했습니다: {name}")
    return tag


def create_or_load(directory: str, name: str, asset_class) -> tuple:
    path = f"{directory}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, asset_class, factory)
    if asset is None:
        raise RuntimeError(f"에셋을 만들지 못했습니다: {path}")
    return asset, True


def set_default(blueprint_path: str, property_name: str, value) -> None:
    blueprint = unreal.load_asset(blueprint_path)
    if blueprint is None:
        raise RuntimeError(f"BP가 없습니다: {blueprint_path}")
    cdo = unreal.get_default_object(blueprint.generated_class())
    if cdo.get_editor_property(property_name) == value:
        unreal.log(f"{blueprint_path}.{property_name}: 이미 연결됨")
        return
    cdo.set_editor_property(property_name, value)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"BP를 저장하지 못했습니다: {blueprint_path}")
    unreal.log(f"{blueprint_path}.{property_name} -> {value.get_path_name()}")


tuning, tuning_created = create_or_load(TUNING_DIR, TUNING_NAME, unreal.DigetherTuningProfile)
if tuning_created:
    values = tuning.get_editor_property("values")
    for key, value in TUNING_VALUES.items():
        values.set_editor_property(key, value)
    tuning.set_editor_property("values", values)
    unreal.EditorAssetLibrary.save_loaded_asset(tuning, only_if_is_dirty=False)
    unreal.log(f"만듦: {tuning.get_path_name()}")

cargo, cargo_created = create_or_load(ITEM_DIR, CARGO_NAME, unreal.DigetherItemDefinition)
if cargo_created:
    cargo.set_editor_property("display_name", "시험 회수품")
    cargo.set_editor_property("item_type", make_tag("Item.Type.Cargo"))
    cargo.set_editor_property("value_credits", 40)
    cargo.set_editor_property("weight_kg", 5.0)
    unreal.EditorAssetLibrary.save_loaded_asset(cargo, only_if_is_dirty=False)
    unreal.log(f"만듦: {cargo.get_path_name()}")

set_default(GAME_MODE_BP, "tuning_profile", tuning)
set_default(PICKUP_BP, "item_definition", cargo)
unreal.log("CREATE_DATA_ASSETS_DONE")
