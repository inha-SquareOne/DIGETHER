"""Unreal MCP ProgrammaticToolset에서 새 L_Hub의 고정 구조와 집기를 작성한다.

실행 전 재질과 메시를 도입하고 맵을 저장한다. 기존 Hub_Architecture가
있으면 중복 제작을 중단한다. 게임플레이 액터와 원본 맵은 수정하지 않는다.
"""

import json
import math


def current_level():
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.get_current_level", "{}")['returnValue']


def find_actors(name):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.find_actors", json.dumps({
        "root": None, "name": name, "actor_type": None, "tag": "", "bounds": None,
        "collision_channels": []}))['returnValue']


def spawn_actor(name):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_class", json.dumps({
        "actor_type": {"refPath": "/Script/Engine.Actor"}, "name": name,
        "xform": {"location": {"x": 0, "y": 0, "z": 0},
                  "rotation": {"pitch": 0, "yaw": 0, "roll": 0},
                  "scale": {"x": 1, "y": 1, "z": 1}},
        "parent": None, "snap_to_ground": False}))['returnValue']


def add_component(owner, name):
    return execute_tool("editor_toolset.toolsets.actor.ActorTools.add_component", json.dumps({
        "owner": owner, "component_type": {"refPath": "/Script/Engine.StaticMeshComponent"},
        "name": name}))['returnValue']


def set_properties(instance, values):
    result = execute_tool("editor_toolset.toolsets.object.ObjectTools.set_properties", json.dumps({
        "instance": instance, "values": json.dumps(values)}))
    if not result['returnValue']:
        raise ValueError("프로퍼티 적용 실패: " + instance['refPath'])


def set_folder(actor, folder):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.set_actor_folder", json.dumps({
        "actor": actor, "folder_path": folder}))


def get_components(actor):
    return execute_tool("editor_toolset.toolsets.actor.ActorTools.get_components", json.dumps({
        "actor": actor, "component_type": None}))['returnValue']


def run():
    if current_level() != "/Game/Game/Maps/L_Hub":
        raise ValueError("새 L_Hub 맵에서만 실행할 수 있습니다.")
    if find_actors("Hub_Architecture"):
        raise ValueError("기존 Hub_Architecture가 있습니다. 중복 실행을 중단합니다.")
    mesh_path = "/Game/Game/Environment/Hub/Meshes/"
    material_path = "/Game/Game/Environment/Hub/Materials/"
    actors = {}
    component_count = 0
    lanterns = []

    def group(name, folder):
        actor = spawn_actor(name)
        actors[name] = actor
        set_folder(actor, folder)
        return actor

    architecture = group("Hub_Architecture", "Hub/Architecture")
    floor = group("Hub_FloorTiles", "Hub/Architecture")
    trim = group("Hub_BrassTrim", "Hub/Architecture")
    furniture = group("Hub_Furniture", "Hub/Furniture")
    altar = group("Hub_Altar", "Hub/Furniture")
    lamps = group("Hub_Lanterns", "Hub/Lighting")
    roof = group("Hub_Roof", "Hub/Roof")
    initialized_roots = []

    def mesh(actor, name, asset, location, scale=(1, 1, 1), material="StoneA", yaw=0, pitch=0,
             collision=True):
        nonlocal component_count
        component = add_component(actor, name)
        if actor['refPath'] not in initialized_roots:
            for root in get_components(actor):
                if root['refPath'] != component['refPath']:
                    set_properties(root, {"mobility": "Static"})
            initialized_roots.append(actor['refPath'])
        asset_ref = asset if asset.startswith("/") else mesh_path + asset + "." + asset
        material_name = "MI_Hub" + material
        # Authoring follows the plan's north-up Y; Unreal north is -Y for the
        # requested plan capture. The imported half-disc already points to -Y.
        engine_yaw = -yaw + 180 if asset == "SM_HubHalfDiscFloor" else -yaw
        engine_location = (location[0], -location[1], location[2])
        values = {
            "staticMesh": {"refPath": asset_ref},
            "overrideMaterials": [{"refPath": material_path + material_name + "." + material_name}],
            "relativeLocation": dict(zip(("x", "y", "z"), engine_location)),
            "relativeRotation": {"pitch": pitch, "yaw": engine_yaw, "roll": 0},
            "relativeScale3D": dict(zip(("x", "y", "z"), scale)),
            "mobility": "Static",
            "bodyInstance": {"collisionProfileName": "BlockAll" if collision else "NoCollision",
                             "collisionEnabled": "QueryAndPhysics" if collision else "NoCollision"}}
        set_properties(component, values)
        component_count += 1
        return component

    def cube(actor, name, pos, size, material="StoneA", yaw=0, collision=True, bevel=True, pitch=0):
        asset = "SM_HubBeveledCube" if bevel else "/Engine/BasicShapes/Cube.Cube"
        return mesh(actor, name, asset, pos, tuple(value / 100 for value in size), material,
                    yaw, pitch, collision)

    # Continuous structural floors close the narrow decorative tile joints.
    cube(architecture, "LivingFoundation", (0, 0, -12), (1800, 900, 16), "StoneC", bevel=False)
    cube(architecture, "SanctuaryFoundation", (0, 755, -12), (900, 610, 16), "StoneC", bevel=False)
    mesh(architecture, "NorthFoundation", "SM_HubHalfDiscFloor", (0, 1060, 0), material="FloorA", yaw=180)
    cube(architecture, "ElevatorFoundation", (0, -575, -4), (400, 250, 16), "DarkMetal", bevel=False)
    for row in range(9):
        for column in range(18):
            x, y = -850 + column * 100, -400 + row * 100
            material = "FloorA" if (column + row * 3) % 5 else "FloorB"
            cube(floor, "LivingTile_%02d_%02d" % (row, column), (x, y, -2),
                 (99.4, 99.4, 4), material, bevel=False)
    for row in range(6):
        for column in range(9):
            cube(floor, "SanctuaryTile_%02d_%02d" % (row, column),
                 (-400 + column * 100, 500 + row * 100, -2), (99.4, 99.4, 4),
                 "FloorB" if (row + column) % 6 == 0 else "FloorA", bevel=False)
    for column in range(9):
        cube(floor, "SanctuaryThreshold_%02d" % column, (-400 + column * 100, 1055, -2),
             (99.4, 9.4, 4), "FloorA", bevel=False)

    def wall(name, start, end, height):
        dx, dy = end[0] - start[0], end[1] - start[1]
        length = math.sqrt(dx * dx + dy * dy)
        yaw = math.degrees(math.atan2(dy, dx))
        mx, my = (start[0] + end[0]) / 2, (start[1] + end[1]) / 2
        cube(architecture, name + "Core", (mx, my, height / 2), (length + 2, 25, height),
             "StoneC", yaw, bevel=False)
        # Five staggered courses retain simple, readable masonry at the reference scale.
        course_height = height / 5
        unit = 115
        for row in range(5):
            cursor = 0
            index = 0
            first_length = unit / 2 if row % 2 else unit
            while cursor < length - 0.1:
                segment = min(first_length if index == 0 else unit, length - cursor)
                if length - cursor - segment < 20:
                    segment = length - cursor
                fraction = (cursor + segment / 2) / length
                px, py = start[0] + dx * fraction, start[1] + dy * fraction
                cube(architecture, "%sStone_%d_%d" % (name, row, index),
                     (px, py, course_height * (row + 0.5)),
                     (max(segment - 1, 1), 29, course_height - 1.5),
                     ("StoneA", "StoneB", "StoneC")[(row + index) % 3], yaw)
                cursor += segment
                index += 1
        cube(trim, name + "Foot", (mx, my, 12), (length + 2, 35, 24), "StoneB", yaw)
        cube(trim, name + "Band", (mx, my, height - 21), (length + 2, 32, 5), "Brass", yaw)
        cube(trim, name + "Cap", (mx, my, height - 6), (length + 2, 36, 12), "StoneB", yaw)

    wall("West", (-912.5, -450), (-912.5, 450), 300)
    wall("East", (912.5, -450), (912.5, 450), 300)
    wall("LivingNorthWest", (-925, 462.5), (-450, 462.5), 300)
    wall("LivingNorthEast", (450, 462.5), (925, 462.5), 300)
    wall("LivingSouthWest", (-925, -462.5), (-200, -462.5), 300)
    wall("LivingSouthEast", (200, -462.5), (925, -462.5), 300)
    wall("SanctuaryWest", (-462.5, 450), (-462.5, 1060), 280)
    wall("SanctuaryEast", (462.5, 450), (462.5, 1060), 280)
    wall("ElevatorWest", (-212.5, -450), (-212.5, -700), 280)
    wall("ElevatorEast", (212.5, -700), (212.5, -450), 280)
    wall("ElevatorSouth", (-225, -712.5), (225, -712.5), 280)
    for index in range(24):
        a0, a1 = math.pi * index / 24, math.pi * (index + 1) / 24
        radius = 462.5 / math.cos(math.pi / 48)
        start = (radius * math.cos(a0), 1060 + radius * math.sin(a0))
        end = (radius * math.cos(a1), 1060 + radius * math.sin(a1))
        wall("NorthArc_%02d" % index, start, end, 280)

    # Roof is one separate group so inspection captures can temporarily hide it.
    cube(roof, "LivingCeiling", (0, 0, 310), (1850, 950, 20), "StoneC", bevel=False)
    cube(roof, "SanctuaryCeiling", (0, 755, 290), (950, 610, 20), "StoneC", bevel=False)
    mesh(roof, "NorthCeiling", "SM_HubHalfDiscFloor", (0, 1060, 300), material="StoneC", yaw=180)
    mesh(roof, "AltarDome", "SM_HubDome", (0, 1060, 0), material="StoneB")
    mesh(roof, "DomeEdgeBand", "SM_HubPoolRim", (0, 1060, 247), (2, 2, .5), "Brass", collision=False)
    cube(roof, "ElevatorCeiling", (0, -575, 290), (450, 250, 20), "StoneC", bevel=False)

    # Doorway piers and narrow brass floor inlay clarify the two shared passages.
    for x, y, h in [(-473.5, 450, 280), (473.5, 450, 280), (-223.5, -450, 280), (223.5, -450, 280)]:
        cube(trim, "Pier_%d_%d" % (x, y), (x, y, h / 2), (40, 45, h), "StoneB")
        for z in (20, h - 25):
            cube(trim, "PierBand_%d_%d_%d" % (x, y, z), (x, y, z), (47, 52, 9), "Brass")
    for side in (-1, 1):
        cube(trim, "FloorLine_%d" % side, (side * 390, 0, 0.7), (500, 5, 1.4), "Brass", collision=False)
        cube(trim, "Diamond_%d" % side, (side * 60, 0, 1), (115, 115, 2), "Brass", yaw=45, collision=False)
        cube(trim, "DiamondInset_%d" % side, (side * 60, 0, 2.1), (94, 94, 1.5), "FloorB", yaw=45, collision=False)
    cube(trim, "CenterDiamond", (0, 0, 3), (40, 40, 1), "Brass", yaw=45, collision=False)

    # Altar pool stays at the drawing's 10cm high rim with a recessed teal surface.
    mesh(altar, "PoolStoneRim", "SM_HubPoolRim", (0, 1060, 0), material="StoneB")
    mesh(altar, "PoolBrassOuterBand", "SM_HubPoolRim", (0, 1060, 4), (1.015, 1.015, 0.2), "Brass", collision=False)
    mesh(altar, "PoolSurface", "SM_HubPoolDisc", (0, 1060, 7), material="TealGlow", collision=False)
    for index in range(8):
        angle = math.pi * 2 * index / 8
        x, y = 211 * math.cos(angle), 1060 + 211 * math.sin(angle)
        cube(altar, "PoolRune_%d" % index, (x, y, 12), (22, 22, 4), "Brass",
             yaw=math.degrees(angle) + 45, collision=False)

    # Counter: low, usable front edge instead of a 2.7m solid block.
    cube(furniture, "DepositBase", (0, 620, 46), (235, 160, 92), "StoneA")
    cube(furniture, "DepositTop", (0, 620, 100), (250, 180, 18), "StoneB")
    for x in (-106, 106):
        cube(furniture, "DepositCorner_%d" % x, (x, 550, 57), (22, 22, 114), "Brass")
    cube(furniture, "DepositFrontPlate", (0, 538, 48), (80, 8, 60), "DarkMetal")
    for x in (-20, 0, 20):
        cube(furniture, "DepositWarmSlit_%d" % x, (x, 532, 48), (6, 3, 35), "OrangeGlow", collision=False)
    cube(furniture, "DepositControl", (-60, 585, 118), (60, 50, 18), "DarkMetal", pitch=-15)
    cube(furniture, "DepositControlGlow", (-60, 581, 130), (42, 34, 2), "OrangeGlow", pitch=-15, collision=False)
    cube(furniture, "DepositSlot", (55, 580, 110), (65, 16, 3), "DarkMetal")
    cube(furniture, "DepositSlotGlow", (55, 580, 112), (45, 5, 2), "OrangeGlow", collision=False)

    for index, y in enumerate((190, -150), 1):
        x = 840
        cube(furniture, "Bed%dBase" % index, (x, y, 20), (120, 180, 40), "Wood")
        cube(furniture, "Bed%dMattress" % index, (x, y, 48), (105, 168, 22), "Bed")
        cube(furniture, "Bed%dBlanket" % index, (x, y - 26, 61), (107, 105, 7), "Bed")
        cube(furniture, "Bed%dPillow" % index, (x, y + 56, 64), (90, 36, 12), "FloorB")
        for dy in (-88, 88):
            cube(furniture, "Bed%dEnd_%d" % (index, dy), (x, y + dy, 41), (120, 10, 82), "Wood")
            cube(furniture, "Bed%dTrim_%d" % (index, dy), (x, y + dy, 84), (123, 13, 7), "Brass")
        for dx in (-55, 55):
            cube(furniture, "Bed%dSide_%d" % (index, dx), (x + dx, y, 36), (8, 180, 9), "Brass")

    cube(furniture, "ShopCabinet", (-837, 80, 48), (126, 140, 96), "Wood")
    cube(furniture, "ShopTop", (-830, 80, 101), (140, 145, 12), "Brass")
    cube(furniture, "ShopScreenHousing", (-820, 100, 127), (63, 65, 22), "DarkMetal", pitch=-20)
    cube(furniture, "ShopScreen", (-818, 100, 141), (44, 46, 2), "TealGlow", pitch=-20, collision=False)
    for y in (35, 125):
        cube(furniture, "ShopDrawer_%d" % y, (-769, y, 52), (5, 65, 40), "StoneB")
        cube(furniture, "ShopHandle_%d" % y, (-765, y, 55), (5, 25, 5), "Brass")

    cube(furniture, "DeliveryBase", (-800, -350, 12), (198, 198, 24), "Brass")
    cube(furniture, "DeliveryChest", (-800, -350, 57), (160, 155, 70), "DarkMetal")
    cube(furniture, "DeliveryLid", (-800, -350, 97), (168, 163, 13), "StoneB")
    for dx in (-57, 57):
        cube(furniture, "DeliveryStrap_%d" % dx, (-800 + dx, -350, 105), (12, 163, 5), "Brass")
    cube(furniture, "DeliveryLatch", (-715, -350, 72), (10, 28, 35), "Brass")

    cube(furniture, "StagePanelPlinth", (250, -400, 15), (96, 96, 30), "StoneB")
    cube(furniture, "StagePanelStem", (250, -410, 65), (50, 50, 80), "DarkMetal")
    cube(furniture, "StagePanelHousing", (250, -400, 118), (92, 67, 20), "Brass", pitch=-25)
    cube(furniture, "StagePanelDisplay", (250, -397, 131), (77, 52, 3), "TealGlow", pitch=-25, collision=False)

    # Elevator is a flush grating and frame; functional trigger remains a separate actor.
    for index in range(10):
        cube(furniture, "ElevatorSlat_%02d" % index, (-180 + index * 40, -575, 2),
             (33, 245, 4), "Wood", collision=False)
    for x in (-185, 185):
        cube(furniture, "ElevatorSideTrim_%d" % x, (x, -575, 7), (20, 250, 14), "Brass")
    for y in (-690, -460):
        cube(furniture, "ElevatorCrossTrim_%d" % y, (0, y, 7), (390, 18, 14), "Brass")
    for angle in (-32, 32):
        cube(furniture, "ElevatorCrossBrace_%d" % angle, (0, -575, 10), (430, 12, 5), "Brass", yaw=angle, collision=False)

    def lantern(name, x, y, yaw):
        cube(lamps, name + "Mount", (x, y, 185), (18, 35, 65), "DarkMetal", yaw)
        cube(lamps, name + "Bracket", (x, y, 165), (48, 48, 12), "DarkMetal", yaw)
        cube(lamps, name + "Glow", (x, y, 195), (21, 24, 31), "OrangeGlow", yaw, collision=False)
        for z in (175, 214):
            cube(lamps, name + "Cap_%d" % z, (x, y, z), (32, 33, 9), "Brass", yaw)
        # Four narrow bars leave the luminous core visible from each side.
        for dx, dy in ((-12, -12), (-12, 12), (12, -12), (12, 12)):
            cube(lamps, "%sBar_%d_%d" % (name, dx, dy), (x + dx, y + dy, 195),
                 (4, 4, 34), "Brass", collision=False)
        lanterns.append({"name": name, "x": x, "y": -y, "z": 192})

    for index, (x, y, yaw) in enumerate([
        (-875, -190, 0), (-875, 320, 0), (875, -300, 0), (875, 370, 0),
        (-650, -422, 0), (600, -422, 0), (-650, 422, 0), (650, 422, 0),
        (-422, 650, 0), (422, 650, 0), (-422, 1010, 0), (422, 1010, 0),
        (-165, -660, 0), (165, -660, 0)]):
        lantern("Lantern_%02d" % index, x, y, yaw)
    for index, angle in enumerate((math.pi / 6, math.pi / 2, math.pi * 5 / 6)):
        lantern("AltarLantern_%d" % index, 415 * math.cos(angle), 1060 + 415 * math.sin(angle),
                math.degrees(angle))

    return {"actors": actors, "component_count": component_count, "lanterns": lanterns,
            "floor_cm": {"living": [1800, 900], "sanctuary": [900, 1060], "elevator": [400, 250]},
            "roof_actor": roof, "saved": False}
