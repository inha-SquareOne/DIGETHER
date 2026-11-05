"""L_Hub 영속 월드에 이동·귀환 확인용 시험장 세 곳을 작성한다.

Unreal MCP ProgrammaticToolset 실행용이다. 시설/보상/채집 규칙이나 추가
엘리베이터 액터는 만들지 않는다. 좌표는 엔진 월드 cm, 북쪽은 -Y다.
실행 전 필요한 메시·재질과 DigetherHubStageAnchor 클래스를 준비한다.
"""

import json
import math


def ref(path):
    return {"refPath": path}


def get_level():
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.get_current_level", "{}")["returnValue"]


def find_actors(name):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.find_actors", json.dumps({
        "root": None, "name": name, "actor_type": None, "tag": "", "bounds": None,
        "collision_channels": []}))["returnValue"]


def spawn(cls, name, location):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_class", json.dumps({
        "actor_type": ref(cls), "name": name,
        "xform": {"location": dict(zip(("x", "y", "z"), location)),
                  "rotation": {"pitch": 0, "yaw": 0, "roll": 0},
                  "scale": {"x": 1, "y": 1, "z": 1}},
        "parent": None, "snap_to_ground": False}))["returnValue"]


def add_component(actor, cls, name):
    return execute_tool("editor_toolset.toolsets.actor.ActorTools.add_component", json.dumps({
        "owner": actor, "component_type": ref(cls), "name": name}))["returnValue"]


def set_props(obj, values):
    result = execute_tool("editor_toolset.toolsets.object.ObjectTools.set_properties", json.dumps({
        "instance": obj, "values": json.dumps(values)}))
    if not result["returnValue"]:
        raise ValueError("시험장 프로퍼티 적용 실패: " + obj["refPath"])


def set_folder(actor):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.set_actor_folder", json.dumps({
        "actor": actor, "folder_path": "Hub/TrialStages"}))


def run():
    if get_level() != "/Game/Game/Maps/L_Hub":
        raise ValueError("시험장은 L_Hub에서만 작성할 수 있습니다.")
    if find_actors("Hub_TrialStageActors") or find_actors("Hub_TrialAnchor_"):
        raise ValueError("기존 시험장이 있습니다. 중복 작성을 중단합니다.")

    actors = []
    anchors = []
    geometry_count = 0
    text_count = 0
    root_initialized = []
    material_prefix = "/Game/Game/Environment/Hub/Materials/"
    mesh_prefix = "/Game/Game/Environment/Hub/Meshes/"

    def cube(actor, name, position, size, material, yaw=0, pitch=0, bevel=False, collision=True):
        nonlocal geometry_count
        component = add_component(actor, "/Script/Engine.StaticMeshComponent", name)
        if actor["refPath"] not in root_initialized:
            set_props(ref(actor["refPath"] + ".DefaultSceneRoot"), {"mobility": "Static"})
            root_initialized.append(actor["refPath"])
        mesh = mesh_prefix + "SM_HubBeveledCube.SM_HubBeveledCube" if bevel else "/Engine/BasicShapes/Cube.Cube"
        material_name = "MI_Hub" + material
        set_props(component, {
            "staticMesh": ref(mesh),
            "overrideMaterials": [ref(material_prefix + material_name + "." + material_name)],
            "relativeLocation": dict(zip(("x", "y", "z"), position)),
            "relativeRotation": {"pitch": pitch, "yaw": yaw, "roll": 0},
            "relativeScale3D": dict(zip(("x", "y", "z"), (value / 100 for value in size))),
            "mobility": "Static", "bodyInstance": {
                "collisionProfileName": "BlockAll" if collision else "NoCollision",
                "collisionEnabled": "QueryAndPhysics" if collision else "NoCollision"}})
        geometry_count += 1

    def light(stage, anchor, index, local, color, intensity):
        actor = spawn("/Script/Engine.PointLight", "Hub_TrialLight_" + stage + "_" + str(index),
                      (anchor[0] + local[0], anchor[1] + local[1], anchor[2] + local[2]))
        set_props(ref(actor["refPath"] + ".LightComponent0"), {
            "mobility": "Movable", "intensityUnits": "Unitless", "bUseInverseSquaredFalloff": False,
            "intensity": intensity, "attenuationRadius": 1000, "lightFalloffExponent": 2,
            "lightColor": dict(zip(("r", "g", "b", "a"), color + (1,))),
            "castShadows": False, "sourceRadius": 20})
        set_folder(actor)
        actors.append(actor)

    def sign(actor, text):
        nonlocal text_count
        component = add_component(actor, "/Script/Engine.TextRenderComponent", "TrialInstructions")
        set_props(component, {
            "text": text, "worldSize": 32,
            "horizontalAlignment": "EHTA_Center", "verticalAlignment": "EVRTA_TextCenter",
            "textRenderColor": {"r": .9, "g": .78, "b": .56, "a": 1},
            "relativeLocation": {"x": 0, "y": -1770, "z": 200},
            "relativeRotation": {"pitch": 0, "yaw": 90, "roll": 0},
            "mobility": "Static", "castShadow": False})
        text_count += 1

    stages = [
        ("MineFree", (12000, 0, 0), "MINE TRIAL", "StoneC", "FloorB", (1, .68, .38), 2.5),
        ("Ruins", (24000, 0, 0), "RUINS TRIAL", "StoneB", "FloorA", (.65, .75, .86), 3),
        ("Crystal", (36000, 0, 0), "CRYSTAL TRIAL", "DarkMetal", "FloorB", (.25, 1, .84), 3)]

    for stage, origin, title, wall_material, floor_material, light_color, intensity in stages:
        actor = spawn("/Script/Engine.Actor", "Hub_TrialStageActors_" + stage, origin)
        actors.append(actor)
        set_folder(actor)
        anchor = spawn("/Script/DIGETHER.DigetherHubStageAnchor", "Hub_TrialAnchor_" + stage, origin)
        set_props(anchor, {"stageId": stage})
        set_folder(anchor)
        anchors.append(anchor)
        actors.append(anchor)

        # Main hall is 20x18m. Its southern opening meets the 4x2.5m elevator cab.
        cube(actor, "RoomFloor", (0, -900, -10), (2000, 1800, 20), floor_material)
        cube(actor, "WestWall", (-1012.5, -900, 150), (25, 1800, 300), wall_material)
        cube(actor, "EastWall", (1012.5, -900, 150), (25, 1800, 300), wall_material)
        cube(actor, "NorthWall", (0, -1812.5, 150), (2050, 25, 300), wall_material)
        cube(actor, "SouthWestWall", (-600, 12.5, 150), (800, 25, 300), wall_material)
        cube(actor, "SouthEastWall", (600, 12.5, 150), (800, 25, 300), wall_material)
        cube(actor, "RoomCeiling", (0, -900, 310), (2050, 1850, 20), "StoneC")
        cube(actor, "CabFloor", (0, 0, -4), (400, 250, 16), "DarkMetal")
        cube(actor, "CabWestWall", (-212.5, 0, 140), (25, 250, 280), "StoneB")
        cube(actor, "CabEastWall", (212.5, 0, 140), (25, 250, 280), "StoneB")
        cube(actor, "CabBackWall", (0, 137.5, 140), (450, 25, 280), "StoneB")
        cube(actor, "CabCeiling", (0, 0, 290), (450, 300, 20), "StoneC")
        for side in (-1, 1):
            cube(actor, "CabEntrancePost_" + str(side), (side * 217, -125, 140),
                 (34, 34, 280), "Brass", bevel=True)
        cube(actor, "CabEntranceHeader", (0, -125, 270), (468, 34, 20), "Brass", bevel=True)
        cube(actor, "CabDoorStripe", (0, -117, 5), (396, 12, 2), "Brass", collision=False)
        cube(actor, "ReturnPath", (0, -700, 1), (8, 1150, 2), "Brass", collision=False)

        if stage == "MineFree":
            # Wood supports define two side bays while preserving a wide central path.
            for row, y in enumerate((-550, -1150)):
                for side in (-1, 1):
                    cube(actor, "TimberPost_%d_%d" % (row, side), (side * 730, y, 140),
                         (50, 60, 280), "Wood", bevel=True)
                cube(actor, "TimberHeader_%d" % row, (0, y, 270), (1510, 65, 45), "Wood", bevel=True)
            for side in (-1, 1):
                cube(actor, "MineRail_" + str(side), (side * 95, -1000, 3),
                     (8, 1350, 6), "DarkMetal", collision=False)
            cube(actor, "MineRockMass", (-850, -1520, 68), (210, 270, 136), "StoneA", yaw=18, bevel=True)
        elif stage == "Ruins":
            for index, (x, y) in enumerate(((-650, -650), (650, -650), (-650, -1300), (650, -1300))):
                cube(actor, "RuinColumn_%d" % index, (x, y, 145), (100, 100, 290), "StoneA", bevel=True)
                cube(actor, "RuinCapital_%d" % index, (x, y, 265), (145, 145, 35), "Brass", bevel=True)
            cube(actor, "RuinPlinth", (0, -1450, 18), (230, 230, 36), "StoneB", yaw=45, bevel=True)
        else:
            for index in range(6):
                side = -1 if index % 2 == 0 else 1
                row = index // 2
                height = 145 + row * 35
                cube(actor, "Crystal_%d" % index, (side * (620 + row * 45), -500 - row * 470, height / 2),
                     (70 + row * 10, 70 + row * 10, height), "TealGlow",
                     yaw=30 * index, pitch=side * 9, bevel=True)
            for side in (-1, 1):
                cube(actor, "CrystalInlay_" + str(side), (side * 420, -970, 1),
                     (10, 1380, 2), "TealGlow", collision=False)
            cube(actor, "CrystalPlinth", (0, -1450, 18), (230, 230, 36), "DarkMetal", yaw=45, bevel=True)

        sign(actor, title + "\nRETURN: ELEVATOR ASCENT LEVER")
        for index, position in enumerate(((-650, -450, 225), (650, -450, 225),
                                          (-650, -1150, 225), (650, -1150, 225),
                                          (0, -1550, 225), (0, 0, 210))):
            light(stage, origin, index, position, light_color, intensity)

    return {"actors": actors, "anchors": anchors, "actor_count": len(actors),
            "geometry_components": geometry_count, "text_components": text_count,
            "stage_ids": [stage[0] for stage in stages], "saved": False}
