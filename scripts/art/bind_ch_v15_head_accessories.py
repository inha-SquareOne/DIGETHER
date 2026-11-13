"""캐릭터 v1.5 리깅 파일의 머리 장식(눈·머리카락·수염·콧수염·모자)을 head 본에 고정해 새 FBX로 내보낸다(Blender 5.2).

원본 리깅 파일(Ch_Model_v1.5_body_rigged.blend)은 머리 장식을 REFERENCE_Excluded_Head_Accessories 컬렉션에
가중치 없이 두고 내보내기에서 뺐다. 이 스크립트는 원본을 저장하지 않고 메모리에서만 바꿔 새 FBX를 만든다.

실행:
  blender -b <Ch_Model_v1.5_body_rigged.blend> --python <이 파일> -- <출력 FBX 경로>

- 각 장식의 모든 정점을 head 본에 가중치 1.0으로 묶는다(턱 본이 없어 수염은 머리와 함께만 움직인다)
- 머티리얼: 눈 → M_Ch_v15_body, 머리카락·수염·콧수염 → M_Ch_v15_hair, 모자 → M_Ch_v15_outfit
- 내보내기는 파일의 unit_contract(1 unit = 1 cm, FBX global_scale 1)를 따른다
"""

import sys
import bpy

ACCESSORIES = {
    "01_eye.001": "SK_Eye",
    "02_hair.001": "SK_Hair",
    "03_Beard.001": "SK_Beard",
    "04_Mustache.001": "SK_Mustache",
    "05_Hat.001": "SK_Hat",
}
HEAD_BONE = "head"
HAIR_MATERIAL = "M_Ch_v15_hair"
MATERIAL_MAP = {
    "01 - Default": "M_Ch_v15_body",
    "Material #42": HAIR_MATERIAL,
    "Material #43": "M_Ch_v15_outfit",
}
EXPORT_MESHES = ["SK_Body", "SK_Pants", "SK_Shoes", "SK_Top"] + list(ACCESSORIES.values())


def material_for(name: str) -> bpy.types.Material:
    base = name.rsplit(".", 1)[0] if name.rsplit(".", 1)[-1].isdigit() else name
    target = MATERIAL_MAP.get(base)
    if target is None:
        raise RuntimeError(f"매핑 없는 머티리얼: {name}")
    mat = bpy.data.materials.get(target)
    if mat is None:
        # 머리카락 머티리얼은 원본에 없어 기존 Material #42를 복제해 이름만 붙인다(텍스처는 엔진에서 지정)
        mat = bpy.data.materials[name].copy()
        mat.name = target
    return mat


def bind_accessories(arm: bpy.types.Object) -> None:
    if HEAD_BONE not in arm.data.bones:
        raise RuntimeError(f"{HEAD_BONE} 본이 없다")
    for src, dst in ACCESSORIES.items():
        obj = bpy.data.objects[src]
        for coll in list(obj.users_collection):
            coll.objects.unlink(obj)
        bpy.context.scene.collection.objects.link(obj)
        obj.name = dst
        obj.hide_render = False
        obj.hide_viewport = False
        obj.hide_set(False)

        world = obj.matrix_world.copy()
        obj.parent = arm
        obj.matrix_world = world

        obj.vertex_groups.clear()
        group = obj.vertex_groups.new(name=HEAD_BONE)
        group.add([v.index for v in obj.data.vertices], 1.0, "REPLACE")
        mod = obj.modifiers.new("Armature", "ARMATURE")
        mod.object = arm

        for slot in obj.material_slots:
            if slot.material is not None:
                slot.material = material_for(slot.material.name)
        print(f"[bind] {src} -> {dst}: 정점 {len(obj.data.vertices)}개 head 고정, "
              f"머티리얼 {[s.material.name for s in obj.material_slots]}")


def export(path: str, arm: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    for name in EXPORT_MESHES:
        bpy.data.objects[name].select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        object_types={"ARMATURE", "MESH"},
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_NONE",
        bake_space_transform=False,
        mesh_smooth_type="FACE",
        use_mesh_modifiers=False,
        # 원본 SK_Ch_v15_Body.fbx가 삼각화돼 있어 같게 맞춘다(비교 시 SK_Body 4024면)
        use_triangles=True,
        add_leaf_bones=False,
        primary_bone_axis="Y",
        secondary_bone_axis="X",
        use_armature_deform_only=False,
        armature_nodetype="NULL",
        bake_anim=False,
        path_mode="STRIP",
    )
    print(f"[bind] 내보냄: {path}")


def main() -> None:
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if not argv:
        raise SystemExit("출력 FBX 경로가 필요하다")
    arm = bpy.data.objects["Armature"]
    bind_accessories(arm)
    export(argv[0], arm)


main()
