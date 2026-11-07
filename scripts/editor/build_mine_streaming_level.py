"""L_Hub MineFree를 Always Loaded L_Mine으로 옮기고 갱도를 확장한다.

실행은 부모 에디터 작업자 한 명만 담당한다. 기본 run()은 읽기 전용이다.
run_phase('build')는 L_Mine이 없을 때만 실행하며 지정된 두 맵만 저장한다.
run_phase('verify', reload=True)는 저장 상태를 확인한 뒤 L_Hub를 다시 연다.
UE 5.8.4 EditorLevelUtils/LevelEditorSubsystem API를 사용한다.
"""
import json
import unreal

HUB = '/Game/Game/Maps/L_Hub'
MINE = '/Game/Game/Maps/L_Mine'
MAT_ROOT = '/Game/Game/Environment/Hub/Materials/'
ALLOWED_PACKAGES = {HUB, MINE}
LAST_BUILD = None


def require(condition, detail):
    if not condition:
        raise RuntimeError(str(detail))


def vec(values):
    return unreal.Vector(*values)


def xyz(value):
    return [round(float(value.x), 5), round(float(value.y), 5), round(float(value.z), 5)]


def transform_record(transform):
    location, rotation, scale = transform.translation, transform.rotation.rotator(), transform.scale3d
    return {'location': xyz(location), 'rotation': [round(float(rotation.pitch), 5),
            round(float(rotation.yaw), 5), round(float(rotation.roll), 5)], 'scale': xyz(scale)}


def dirty_packages():
    packages = list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    packages += list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    return sorted({package.get_path_name() for package in packages})


def editor_context():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    require(not level_editor.is_in_play_in_editor(), 'PIE를 먼저 끝낸 뒤 실행하세요.')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    require(world and world.get_path_name().split('.')[0] == HUB, 'L_Hub가 열려 있어야 합니다.')
    return world, level_editor, unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def actor_record(actor):
    return {'path': actor.get_path_name(), 'label': actor.get_actor_label(),
            'class': actor.get_class().get_path_name(), 'level': actor.get_level().get_path_name(),
            'transform': transform_record(actor.get_actor_transform())}


def components(actor):
    return list(actor.get_components_by_class(unreal.StaticMeshComponent))


def is_anchor(actor):
    return actor.get_class().get_name() == 'DigetherHubStageAnchor'


def mine_anchor(actors):
    anchors = [actor for actor in actors if is_anchor(actor)
               and str(actor.get_editor_property('stage_id')) == 'MineFree']
    require(len(anchors) == 1, {'MineFree_anchor_count': len(anchors)})
    return anchors[0]


def mine_actors(actors, explicit_paths=None):
    if explicit_paths:
        require(len(explicit_paths) == len(set(explicit_paths)) == 8, '이동 경로는 고유한 8개여야 합니다.')
        found = {actor.get_path_name(): actor for actor in actors}
        require(set(explicit_paths).issubset(found), '이동 경로가 현재 에디터에 없습니다.')
        selected = [found[path] for path in explicit_paths]
    else:
        selected = [actor for actor in actors if 'MineFree' in actor.get_actor_label()
                    or (is_anchor(actor) and str(actor.get_editor_property('stage_id')) == 'MineFree')]
    require(len(selected) == 8, {'expected_mine_actors': 8, 'found': [actor_record(a) for a in selected]})
    anchor = mine_anchor(selected)
    geometry = [actor for actor in selected if {'RoomFloor', 'NorthWall', 'MineRockMass'}.issubset(
        {component.get_name() for component in components(actor)})]
    require(len(geometry) == 1, '기존 MineFree 지오메트리 액터를 고유하게 확인할 수 없습니다.')
    require(sum(isinstance(actor, unreal.PointLight) for actor in selected) == 6, '기존 MineFree 라이트 6개가 필요합니다.')
    require(all(actor.get_level().get_path_name().startswith(HUB + '.') for actor in selected),
            '이동할 액터가 모두 L_Hub 영속 레벨에 있어야 합니다.')
    return selected, anchor, geometry[0]



def elevator_floor(actors):
    elevators = [actor for actor in actors if 'ElevatorActor' in actor.get_class().get_name()]
    require(len(elevators) == 1, {'elevator_actor_count': len(elevators)})
    actor = elevators[0]
    require(actor.get_level().get_path_name().startswith(HUB + '.'), '기존 엘리베이터는 L_Hub에 남아 있어야 합니다.')
    floors = [component for component in components(actor) if component.get_name() == 'FloorMesh']
    require(len(floors) == 1, '기존 엘리베이터 FloorMesh를 고유하게 찾을 수 없습니다.')
    return actor, floors[0]


def floor_record(actor, component):
    return {'actor': actor_record(actor), 'component': component.get_name(),
            'collision_enabled': str(component.get_collision_enabled()),
            'collision_profile': str(component.get_collision_profile_name())}


def mine_streams(world):
    # UE 5.8.4 Python은 World.streaming_levels를 노출하지 않아 패키지 이름으로 조회
    stream = unreal.GameplayStatics.get_streaming_level(world, MINE)
    return [stream] if stream else []


def streaming_record(stream):
    return {'path': stream.get_path_name(), 'class': stream.get_class().get_name(),
            'package': str(stream.get_world_asset_package_f_name()),
            'transform': transform_record(stream.get_editor_property('level_transform')),
            'loaded': bool(stream.get_loaded_level()),
            'visible_in_editor': bool(stream.is_level_visible()),
            'should_be_loaded': bool(stream.is_level_loaded()),
            'should_be_visible': bool(stream.get_editor_property('should_be_visible'))}



def api_probe(world=None):
    """쓰기 전에 Python 노출 여부와 필요한 읽기 프로퍼티를 확인한다."""
    if world is None:
        world, _, _ = editor_context()
    errors = []
    methods = {
        unreal.EditorLevelUtils: ('create_new_streaming_level', 'move_actors_to_level', 'make_level_current'),
        unreal.LevelEditorSubsystem: ('get_current_level', 'set_current_level_by_name', 'load_level', 'is_in_play_in_editor'),
        unreal.EditorActorSubsystem: ('get_all_level_actors', 'spawn_actor_from_class'),
        unreal.EditorLoadingAndSavingUtils: ('save_map', 'get_dirty_map_packages', 'get_dirty_content_packages'),
        unreal.LevelStreamingAlwaysLoaded: ('get_world_asset_package_f_name', 'get_loaded_level',
                                           'is_level_loaded', 'is_level_visible'),
        unreal.SystemLibrary: ('get_component_bounds',),
        unreal.StaticMeshComponent: ('set_static_mesh', 'set_material', 'set_collision_profile_name',
            'set_collision_enabled', 'get_collision_enabled', 'get_collision_profile_name', 'set_mobility', 'set_relative_scale3d',
            'set_visibility', 'set_hidden_in_game'),
        unreal.Actor: ('get_level', 'get_actor_transform', 'get_components_by_class',
                      'get_component_by_class', 'set_actor_scale3d', 'set_folder_path', 'set_actor_label'),
        unreal.PointLightComponent: ('set_intensity', 'set_attenuation_radius', 'set_light_color', 'set_cast_shadows'),
    }
    for owner, names in methods.items():
        for name in names:
            if not callable(getattr(owner, name, None)):
                errors.append(owner.__name__ + '.' + name)
    samples = [
        (unreal.get_default_object(unreal.LevelStreamingAlwaysLoaded),
         ('level_transform', 'should_be_visible')),
    ]
    light_cdo = unreal.get_default_object(unreal.PointLight).get_component_by_class(unreal.PointLightComponent)
    mesh_cdo = unreal.get_default_object(unreal.StaticMeshActor).get_component_by_class(unreal.StaticMeshComponent)
    samples += [(light_cdo, ('intensity_units', 'use_inverse_squared_falloff')),
                (mesh_cdo, ('relative_location', 'relative_scale3d'))]
    for obj, names in samples:
        for name in names:
            try:
                obj.get_editor_property(name)
            except Exception as error:
                errors.append(str(obj) + '.' + name + ': ' + str(error))
    try:
        transform_record(unreal.Transform())
    except Exception as error:
        errors.append('break_transform: ' + str(error))
    return {'ready': not errors, 'errors': errors}


def inspect():
    world, _, actor_subsystem = editor_context()
    actors = list(actor_subsystem.get_all_level_actors())
    nearby = [actor_record(actor) for actor in actors
              if abs(actor.get_actor_location().x - 12000) < 3000]
    anchors = [actor_record(actor) | {'stage_id': str(actor.get_editor_property('stage_id'))}
               for actor in actors if is_anchor(actor)]
    elevators = []
    for actor in actors:
        if 'ElevatorActor' in actor.get_class().get_name():
            elevators.append(actor_record(actor) | {'mesh_collision': [
                {'name': component.get_name(), 'enabled': str(component.get_collision_enabled()),
                 'profile': str(component.get_collision_profile_name())}
                for component in components(actor)]})
    return {'phase': 'inspect', 'engine': unreal.SystemLibrary.get_engine_version(),
            'mine_exists': unreal.EditorAssetLibrary.does_asset_exist(MINE),
            'streaming_levels': [streaming_record(s) for s in mine_streams(world)],
            'anchors': anchors, 'nearby_mine_actors': nearby, 'elevators': elevators,
            'dirty_packages': dirty_packages(), 'api_probe': api_probe(world)}


def collision_overlap(component, origin):
    if component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
        return False
    center, extent, _ = unreal.SystemLibrary.get_component_bounds(component)
    low = [center.x - extent.x - origin.x, center.y - extent.y - origin.y,
           center.z - extent.z - origin.z]
    high = [center.x + extent.x - origin.x, center.y + extent.y - origin.y,
            center.z + extent.z - origin.z]
    # 바닥 상면 Z=0과 경계면은 제외하고 실제 도착 공간만 검사한다.
    return all(high[i] > (-200, -125, 0.5)[i] + 0.01 and
               low[i] < (200, 125, 300)[i] - 0.01 for i in range(3))


def verify(reload=False):
    world, level_editor, actor_subsystem = editor_context()
    if reload:
        require(not (set(dirty_packages()) & ALLOWED_PACKAGES), '두 맵의 미저장 변경을 먼저 저장하세요.')
        require(level_editor.load_level(HUB), 'L_Hub 다시 열기 실패')
        world, level_editor, actor_subsystem = editor_context()
    actors = list(actor_subsystem.get_all_level_actors())
    streams = mine_streams(world)

    require(len(streams) == 1, {'L_Mine_streaming_count': len(streams)})
    stream = streams[0]
    record = streaming_record(stream)
    require(record['class'] == 'LevelStreamingAlwaysLoaded', record)
    require(record['transform'] == {'location': [0.0, 0.0, 0.0],
            'rotation': [0.0, 0.0, 0.0], 'scale': [1.0, 1.0, 1.0]}, record)
    require(record['loaded'] and record['visible_in_editor'], record)

    anchor = mine_anchor(actors)
    require(anchor.get_level() == stream.get_loaded_level(), 'MineFree 앵커가 L_Mine 소속이 아닙니다.')
    require(xyz(anchor.get_actor_scale3d()) == [1.0, 1.0, 1.0], '앵커 스케일이 1이 아닙니다.')
    mine_level_actors = [actor for actor in actors if actor.get_level() == stream.get_loaded_level()]
    require(sum(is_anchor(actor) for actor in mine_level_actors) == 1, 'L_Mine 앵커가 하나가 아닙니다.')
    forbidden = [actor_record(actor) for actor in mine_level_actors if isinstance(actor, unreal.PlayerStart)
                 or any(name in actor.get_class().get_name() for name in ('ElevatorActor', 'ElevatorLeverActor'))]
    require(not forbidden, {'forbidden_mine_actors': forbidden})
    # UE 5.8.4 Python은 Level.world_settings를 노출하지 않아 GameMode Override 검사는 생략(새 레벨 기본값 없음)

    blocked = [{'actor': actor.get_actor_label(), 'component': component.get_name()}
               for actor in mine_level_actors for component in components(actor)
               if collision_overlap(component, anchor.get_actor_location())]
    require(not blocked, {'arrival_obstacles': blocked})
    elevator, floor = elevator_floor(actors)
    require(floor.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
            and str(floor.get_collision_profile_name()) == 'BlockAll',
            {'moving_elevator_floor_collision': floor_record(elevator, floor)})
    moved = []
    if LAST_BUILD:
        by_label = {actor.get_actor_label(): actor for actor in actors}
        for label, original in LAST_BUILD['moved_transforms'].items():
            require(label in by_label, {'missing_moved_actor': label})
            actor = by_label[label]
            require(actor.get_level() == stream.get_loaded_level(), {'wrong_level': label})
            require(transform_record(actor.get_actor_transform()) == original, {'moved_transform_changed': label})
            moved.append(label)
        require(actor_record(anchor)['transform'] == LAST_BUILD['anchor_transform'], '앵커 Transform이 바뀌었습니다.')
    return {'phase': 'verify', 'reloaded': reload, 'streaming': record,
            'anchor': actor_record(anchor), 'mine_actor_count': len(mine_level_actors),
            'moved_actors_checked': moved, 'arrival_obstacles': blocked,
            'game_mode_override': None, 'elevator_floor': floor_record(elevator, floor),
            'dirty_packages': dirty_packages()}


def build(explicit_paths=None):
    global LAST_BUILD
    world, level_editor, actor_subsystem = editor_context()
    require('5.8.4' in unreal.SystemLibrary.get_engine_version(), 'UE 5.8.4에서만 실행합니다.')
    probe = api_probe(world)
    require(probe['ready'], {'missing_python_api': probe['errors']})
    require(not unreal.EditorAssetLibrary.does_asset_exist(MINE), 'L_Mine이 이미 있어 덮어쓰지 않습니다.')
    require(not mine_streams(world),
            'L_Mine 스트리밍 항목이 이미 있습니다.')
    all_actors = list(actor_subsystem.get_all_level_actors())
    selected, anchor, geometry = mine_actors(all_actors, explicit_paths)
    elevator, floor = elevator_floor(all_actors)
    require(floor.get_editor_property('static_mesh'), '기존 엘리베이터 바닥 메시가 없습니다.')
    floor_before = floor_record(elevator, floor)
    anchor_before = transform_record(anchor.get_actor_transform())
    require(anchor_before['scale'] == [1.0, 1.0, 1.0], '기존 앵커 스케일을 확인하세요.')
    require(anchor_before['rotation'] == [0.0, 0.0, 0.0], '현행 갱도 배치는 기존 무회전 앵커를 전제로 합니다.')
    selected_labels = [actor.get_actor_label() for actor in selected]
    require(len(set(selected_labels)) == 8, '이동할 액터의 label이 중복됩니다.')
    moved_transforms = {actor.get_actor_label(): transform_record(actor.get_actor_transform()) for actor in selected}
    preserved = {actor.get_path_name(): transform_record(actor.get_actor_transform())
                 for actor in all_actors if actor not in selected}
    geometry_label = geometry.get_actor_label()
    initial_dirty = set(dirty_packages())
    mesh = unreal.load_asset('/Engine/BasicShapes/Cube')
    materials = {name: unreal.load_asset(MAT_ROOT + 'MI_Hub' + name)
                 for name in ('StoneA', 'StoneB', 'StoneC', 'FloorB', 'Wood', 'DarkMetal', 'Brass', 'TealGlow', 'OrangeGlow')}
    require(mesh and all(materials.values()), '필요한 기존 큐브/Hub 재질이 없습니다.')
    require(unreal.EditorLoadingAndSavingUtils.save_map(world, HUB), 'L_Hub 변경 전 저장 실패')
    stream = unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingAlwaysLoaded, MINE, False)
    require(stream, 'L_Mine 생성 실패')
    stream.set_editor_property('level_transform', unreal.Transform())



    require(unreal.EditorLevelUtils.move_actors_to_level(selected, stream, False, False) == 8,
            'MineFree 액터 8개 이동 실패. 자동 재실행하지 말고 inspect로 상태를 확인하세요.')
    unreal.EditorLevelUtils.make_level_current(stream)
    require(level_editor.get_current_level() == stream.get_loaded_level(), '현재 편집 레벨이 L_Mine이 아닙니다.')
    current_actors = list(actor_subsystem.get_all_level_actors())
    moved_by_label = {actor.get_actor_label(): actor for actor in current_actors if actor.get_level() == stream.get_loaded_level()}
    require(set(selected_labels).issubset(moved_by_label), '이동 후 label을 찾을 수 없습니다.')
    for label, original in moved_transforms.items():
        require(transform_record(moved_by_label[label].get_actor_transform()) == original,
                {'world_transform_changed_on_move': label})
    geometry = moved_by_label[geometry_label]
    anchor = mine_anchor(current_actors)
    origin = anchor.get_actor_location()
    parts = {component.get_name(): component for component in components(geometry)}
    for name, component in parts.items():
        if name.startswith('Cab') or name == 'NorthWall':
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_visibility(False, False)
            component.set_hidden_in_game(True, False)
        elif name in ('WestWall', 'EastWall', 'SouthWestWall', 'SouthEastWall'):
            relative = component.get_editor_property('relative_location')
            relative.z = 175
            component.set_editor_property('relative_location', relative)
            scale = component.get_editor_property('relative_scale3d')
            scale.z = 3.5
            component.set_relative_scale3d(scale)
        elif name == 'RoomCeiling':
            relative = component.get_editor_property('relative_location')
            relative.z = 360
            component.set_editor_property('relative_location', relative)
            scale = component.get_editor_property('relative_scale3d')
            scale.y = 18.0
            component.set_relative_scale3d(scale)
        elif name.startswith('TimberPost_'):
            relative = component.get_editor_property('relative_location')
            relative.z = 175
            component.set_editor_property('relative_location', relative)
            scale = component.get_editor_property('relative_scale3d')
            scale.z = 3.5
            component.set_relative_scale3d(scale)
        elif name.startswith('TimberHeader_'):
            relative = component.get_editor_property('relative_location')
            relative.z = 325
            component.set_editor_property('relative_location', relative)
            scale = component.get_editor_property('relative_scale3d')
            scale.z = 0.5
            component.set_relative_scale3d(scale)
    created = []
    collision_manifest = []

    def cube(name, local, size, material='StoneC', collision=True, yaw=0, pitch=0):
        position = vec((origin.x + local[0], origin.y + local[1], origin.z + local[2]))
        actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, position,
                                                      unreal.Rotator(pitch, yaw, 0), False)
        require(actor and actor.get_level() == stream.get_loaded_level(), {'spawn_failed': name})
        actor.set_actor_label('Mine_' + name)
        actor.set_folder_path('Mine/Geometry' if collision else 'Mine/Details')
        component = actor.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.STATIC)
        require(component.set_static_mesh(mesh), {'mesh_failed': name})
        component.set_material(0, materials[material])
        component.set_collision_profile_name('BlockAll' if collision else 'NoCollision', True)
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision
                                        else unreal.CollisionEnabled.NO_COLLISION)
        actor.set_actor_scale3d(vec([axis / 100.0 for axis in size]))
        created.append(actor)
        if collision:
            collision_manifest.append({'name': name, 'local': local, 'size': size})
        return actor

    def lamp(name, local):
        actor = actor_subsystem.spawn_actor_from_class(unreal.PointLight,
            vec((origin.x + local[0], origin.y + local[1], origin.z + local[2])), unreal.Rotator(), False)
        require(actor and actor.get_level() == stream.get_loaded_level(), {'lamp_failed': name})
        actor.set_actor_label('Mine_Lantern_' + name)
        actor.set_folder_path('Mine/Lighting')
        light = actor.get_component_by_class(unreal.PointLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
        light.set_editor_property('use_inverse_squared_falloff', True)
        light.set_intensity(1500)
        light.set_attenuation_radius(900)
        light.set_light_color(unreal.LinearColor(1.0, 0.58, 0.25, 1.0), False)
        light.set_cast_shadows(False)
        created.append(actor)
        cube('LanternBody_' + name, local, (18, 18, 28), 'OrangeGlow', False)
        cube('LanternTop_' + name, (local[0], local[1], local[2] + 18), (28, 28, 8), 'DarkMetal', False)

    # 기존 북벽을 두 개로 나누고 4.5m 통로 개구부를 남긴다.
    for side in (-1, 1):
        cube('HallNorthWall_' + str(side), (side * 625, -1812.5, 175), (800, 25, 350))
    cube('MainFloor', (0, -3700, -10), (450, 3800, 20), 'FloorB')
    cube('MainCeiling', (0, -3700, 375), (450, 3800, 50))
    for side in (-1, 1):
        for index, (near, far) in enumerate(((-1800, -3175), (-3625, -5600))):
            cube('MainWall_%s_%s' % (side, index), (side * 250, (near + far) / 2, 175),
                 (50, abs(far - near), 350), 'StoneB' if index else 'StoneC')
        cube('BranchFloor_' + str(side), (side * 1037.5, -3400, -10), (1625, 450, 20), 'FloorB')
        cube('BranchCeiling_' + str(side), (side * 1037.5, -3400, 375), (1625, 450, 50))
        for wall_side in (-1, 1):
            cube('BranchWall_%s_%s' % (side, wall_side),
                 (side * 1037.5, -3400 + wall_side * 250, 175), (1625, 50, 350), 'StoneB')
        room_x = side * 2200
        cube('OreRoomFloor_' + str(side), (room_x, -3400, -10), (700, 700, 20), 'FloorB')
        cube('OreRoomCeiling_' + str(side), (room_x, -3400, 375), (700, 700, 50))
        cube('OreRoomBack_' + str(side), (side * 2575, -3400, 175), (50, 800, 350), 'StoneA')
        for wall_side in (-1, 1):
            cube('OreRoomSide_%s_%s' % (side, wall_side),
                 (room_x, -3400 + wall_side * 375, 175), (700, 50, 350))
            cube('OreRoomEntrance_%s_%s' % (side, wall_side),
                 (side * 1825, -3400 + wall_side * 287.5, 175), (50, 125, 350))
    cube('DeepRoomFloor', (0, -6000, -10), (800, 800, 20), 'FloorB')
    cube('DeepRoomCeiling', (0, -6000, 375), (800, 800, 50))
    cube('DeepRoomBack', (0, -6425, 175), (900, 50, 350), 'StoneA')
    for side in (-1, 1):
        cube('DeepRoomSide_' + str(side), (side * 425, -6000, 175), (50, 800, 350))
        cube('DeepRoomEntrance_' + str(side), (side * 312.5, -5575, 175), (175, 50, 350))
    for index, y in enumerate(range(-2000, -5601, -500)):
        for side in (-1, 1):
            cube('TimberPost_%s_%s' % (index, side), (side * 225, y, 175), (30, 35, 350), 'Wood')
        cube('TimberBeam_' + str(index), (0, y, 330), (500, 40, 40), 'Wood')
    for side in (-1, 1):
        for index, distance in enumerate((700, 1200, 1700)):
            for edge in (-1, 1):
                cube('BranchPost_%s_%s_%s' % (side, index, edge),
                     (side * distance, -3400 + edge * 225, 175), (35, 30, 350), 'Wood')
            cube('BranchBeam_%s_%s' % (side, index), (side * distance, -3400, 330), (40, 500, 40), 'Wood')
    for side in (-1, 1):
        cube('Rail_' + str(side), (side * 70, -3700, 3), (7, 3800, 6), 'DarkMetal', False)
        for branch_side in (-1, 1):
            cube('BranchRail_%s_%s' % (branch_side, side),
                 (branch_side * 1037.5, -3400 + side * 70, 3), (1625, 7, 6), 'DarkMetal', False)
    for index, y in enumerate(range(-1900, -5600, -150)):
        cube('RailSleeper_' + str(index), (0, y, 1.5), (200, 18, 3), 'Wood', False)
    for index, y in enumerate(range(-2000, -6201, -700)):
        lamp('Main_' + str(index), ((-1 if index % 2 else 1) * 170, y, 285))
    for side in (-1, 1):
        for index, x in enumerate((800, 1500, 2200)):
            lamp('Branch_%s_%s' % (side, index), (side * x, -3220, 285))
        for index in range(5):
            cube('OreRock_%s_%s' % (side, index), (side * (2400 + index % 2 * 35), -3630 + index * 110, 50),
                 (140, 100, 100), 'StoneA', True, yaw=17 + index * 13)
            cube('OreCrystal_%s_%s' % (side, index), (side * (2350 + index % 2 * 25), -3630 + index * 110, 110),
                 (35, 40, 75), 'TealGlow' if side < 0 else 'OrangeGlow', False, yaw=index * 31, pitch=side * 14)
    for index, x in enumerate((-280, 0, 280)):
        cube('DeepRock_' + str(index), (x, -6310, 55), (170, 140, 110), 'StoneA', True, yaw=index * 23)
        cube('DeepOre_' + str(index), (x, -6290, 135), (45, 45, 95), 'Brass', False, yaw=index * 37, pitch=12)
    # 고정 CabFloor 대신 함께 순간이동하는 기존 엘리베이터 인스턴스 바닥만 충돌 활성화.
    floor.set_collision_profile_name('BlockAll', True)
    floor.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    require(floor_record(elevator, floor)['actor'] == floor_before['actor'], '엘리베이터 Transform이 바뀌었습니다.')
    LAST_BUILD = {'moved_transforms': moved_transforms, 'anchor_transform': anchor_before,
                  'created_actor_count': len(created), 'collision_manifest': collision_manifest,
                  'elevator_floor_before': floor_before, 'elevator_floor_after': floor_record(elevator, floor)}
    after_actors = {actor.get_path_name(): actor for actor in actor_subsystem.get_all_level_actors()}
    for path, before in preserved.items():
        require(path in after_actors and transform_record(after_actors[path].get_actor_transform()) == before,
                {'unrelated_actor_changed': path})
    require(not (set(dirty_packages()) - initial_dirty - ALLOWED_PACKAGES),
            {'unrelated_new_dirty': sorted(set(dirty_packages()) - initial_dirty - ALLOWED_PACKAGES)})
    verification = verify()
    require(unreal.EditorLoadingAndSavingUtils.save_map(stream.get_loaded_level().get_outer(), MINE), 'L_Mine 저장 실패')
    require(unreal.EditorLoadingAndSavingUtils.save_map(world, HUB), 'L_Hub 저장 실패')
    require(level_editor.set_current_level_by_name('L_Hub'), '현재 레벨 L_Hub 복귀 실패')
    return {'phase': 'build', 'saved': [MINE, HUB], 'moved_actor_labels': selected_labels,
            'created_actor_count': len(created), 'verification': verification,
            'elevator_floor_before': floor_before, 'elevator_floor_after': floor_record(elevator, floor),
            'dirty_packages': dirty_packages(), 'initial_dirty_packages': sorted(initial_dirty)}


def run_phase(phase='inspect', explicit_paths=None, reload=False):
    if phase == 'inspect':
        return inspect()
    if phase == 'build':
        return build(explicit_paths)
    if phase == 'verify':
        return verify(reload)
    raise ValueError('phase는 inspect/build/verify 중 하나여야 합니다.')


def run():
    return run_phase('inspect')
