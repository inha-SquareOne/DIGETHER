"""곡괭이 바닥 크기와 손에 쥐는 위치·방향을 맞춘다(에디터 Python).

실행(에디터를 닫고):
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4
같은 값을 다시 넣으므로 여러 번 실행해도 결과가 같다. 에디터가 열려 있으면 아래 값을 BP·DA에 직접 입력해도 된다.

측정(2026-11-17, SK_Ch_v15_Body 기준 자세):
- hand_r_socket은 손이 아니라 손에서 약 45cm 떨어진 곳(삽 가운데 위치)에 있다. 소켓 공간에서 손 뼈 (45.7, -11.1, 3.9),
  주먹 구멍(검지~새끼 첫 마디 사이, 엄지 안쪽) 약 (48, -27, 0.5). 쥐는 축은 소켓 X, 엄지 쪽이 -X, 손등→손가락 방향이 -Y.
- 곡괭이 메시 wlkkdfz_tier_2: 길이 X 약 90cm, 머리가 +X 끝, 머리 긴 축이 Y, 자루 중심선은 y≈0, z≈2.1.

손에 든 곡괭이(BP_Weapon_Pickaxe.WeaponMesh, 소켓 기준):
- 회전 Roll 180·Yaw 180: 메시 +X(머리) → 소켓 -X(엄지 쪽, 앞), 메시 Y(머리 긴 축) → 소켓 Y(손가락 방향과 나란히, 망치 쥐는 방향)
- 위치: 자루 끝에서 GRIP_FROM_END_CM 위 지점(메시 (-45+GRIP_FROM_END_CM, 0, 2.1))이 주먹 구멍에 오도록 이동
바닥 곡괭이: WorldMeshScale 1(원본 90cm, 삽 88cm와 비슷)
"""

import unreal

EAL = unreal.EditorAssetLibrary
PICKAXE_ITEM = "/Game/Data/Items/DA_Item_Pickaxe"
PICKAXE_BP = "/Game/Game/Weapon/Pickaxe/BP_Weapon_Pickaxe"

WORLD_SCALE = 1.0
GRIP_FROM_END_CM = 10.0
FIST_IN_SOCKET = unreal.Vector(48.0, -27.0, 0.5)
SHAFT_CENTER_Z = 2.1


def log(msg: str) -> None:
    unreal.log(f"[fix_pickaxe_scale_and_grip] {msg}")


def require(path: str):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"에셋이 없습니다: {path}")
    return asset


item = require(PICKAXE_ITEM)
item.set_editor_property("world_mesh_scale", WORLD_SCALE)
EAL.save_asset(PICKAXE_ITEM)
log(f"DA_Item_Pickaxe.WorldMeshScale {WORLD_SCALE}")

# 회전 R: (x, y, z) -> (-x, y, -z). 쥐는 점 p를 주먹 구멍에 두는 이동 t = 주먹 - R(p)
grip_x = -45.0 + GRIP_FROM_END_CM
location = unreal.Vector(FIST_IN_SOCKET.x - (-grip_x), FIST_IN_SOCKET.y - 0.0, FIST_IN_SOCKET.z - (-SHAFT_CENTER_Z))
bp = require(PICKAXE_BP)
component = unreal.get_default_object(bp.generated_class()).get_editor_property("weapon_mesh")
component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
component.set_editor_property("relative_rotation", unreal.Rotator(roll=180.0, pitch=0.0, yaw=180.0))
component.set_editor_property("relative_location", location)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
EAL.save_asset(PICKAXE_BP)
log(f"BP_Weapon_Pickaxe 위치 {location}, 회전 Roll 180·Yaw 180, 배율 1")
