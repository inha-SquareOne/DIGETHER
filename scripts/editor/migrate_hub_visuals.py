"""Split L_Hub's blockout into art-replaceable facility Blueprints via MCP.

Use ProgrammaticTools.execute_tool_script after inspecting registered schemas.
Back up and save L_Hub first. Run build_one(kind) for each KINDS entry, then
capture_baseline() before wiring, then wire_instances(), validate(baseline),
retire_sources(baseline), validate(baseline, retired=True). Persist the baseline
JSON outside the map so the same checks can run after reloading it.
Each creation step refuses existing packages/instances. Retirement happens only
after the replacement geometry and unchanged gameplay transforms are checked.
"""
import json
import math

BASE = 'editor_toolset.toolsets.'
FOLDER = '/Game/Game/Environment/Hub/Blueprints'
LEVEL = '/Game/Game/Maps/L_Hub'
KINDS = ['Shop', 'Delivery', 'Bed', 'Altar', 'Deposit', 'StagePanel', 'Elevator', 'Lever']
# Floor-centred roots. Local +X faces the approach side.
SPECS = [
    ('Shop', 'Shop', (-830, -80, 0), 0, 'Hub_Facility_HubShop'),
    ('Delivery', 'Delivery', (-800, 350, 0), 0, 'Hub_Facility_HubDelivery'),
    ('Bed', 'Bed1', (840, -190, 0), 180, 'Hub_Facility_HubBed1'),
    ('Bed', 'Bed2', (840, 150, 0), 180, 'Hub_Facility_HubBed2'),
    ('Altar', 'Altar', (0, -1060, 0), 90, 'Hub_Facility_HubAltar'),
    ('Deposit', 'Deposit', (0, -620, 0), 90, 'BP_DigetherDepositStation_Test_C_0'),
    ('StagePanel', 'StagePanel', (250, 400, 0), -90, 'Hub_Facility_HubStage'),
    ('Elevator', 'Elevator', (0, 575, 0), -90, 'DigetherElevatorActor_0'),
    ('Lever', 'Lever0', (-150, 590, 0), -90, 'DigetherElevatorLeverActor_0'),
    ('Lever', 'Lever1', (150, 590, 0), -90, 'DigetherElevatorLeverActor_1'),
]
MESH_PROPS = ['staticMesh', 'overrideMaterials', 'relativeLocation',
              'relativeRotation', 'relativeScale3D', 'bodyInstance', 'castShadow']


def ref(path):
    return {'refPath': path}


def call(tool, args):
    return execute_tool(BASE + tool, json.dumps(args))


def props(obj, names):
    return json.loads(call('object.ObjectTools.get_properties',
                           {'instance': obj, 'properties': names})['returnValue'])


def setp(obj, values):
    assert call('object.ObjectTools.set_properties',
                {'instance': obj, 'values': json.dumps(values)})['returnValue']


def one(label):
    if label.startswith(('DigetherElevator', 'BP_DigetherDepositStation')):
        actor = ref(LEVEL + '.L_Hub:PersistentLevel.' + label)
        transform(actor)  # Resolve the saved object name, independent of its UI label.
        return actor
    actors = call('scene.SceneTools.find_actors', {
        'root': None, 'name': label, 'actor_type': None, 'tag': '',
        'bounds': None, 'collision_channels': []})['returnValue']
    exact = [a for a in actors if call('actor.ActorTools.get_label',
                                     {'actor': a})['returnValue'] == label]
    assert len(exact) == 1, (label, exact)
    return exact[0]


def transform(actor):
    return call('actor.ActorTools.get_actor_transform', {'actor': actor})['returnValue']


def components(actor, cls='StaticMeshComponent'):
    return call('actor.ActorTools.get_components', {
        'actor': actor, 'component_type': ref('/Script/Engine.' + cls)})['returnValue']


def name(obj):
    return obj['refPath'].split('.')[-1].split(':')[-1]


def local_point(point, origin, yaw):
    angle = math.radians(yaw)
    x, y = point['x'] - origin[0], point['y'] - origin[1]
    return {'x': x * math.cos(angle) + y * math.sin(angle),
            'y': -x * math.sin(angle) + y * math.cos(angle),
            'z': point['z'] - origin[2]}


def source_meshes(spec):
    kind, label, origin, yaw, gameplay_label = spec
    if kind == 'Lever':
        return components(one(gameplay_label))
    actor = one('Hub_Altar' if kind == 'Altar' else 'Hub_Furniture')
    # Original group actors were authored with identity transforms.
    pose = transform(actor)
    assert all(abs(pose['location'][axis]) < .001 for axis in ['x', 'y', 'z']), pose
    assert all(abs(pose['rotation'][axis]) < .001 for axis in ['pitch', 'yaw', 'roll']), pose
    prefix = 'Pool' if kind == 'Altar' else label if kind == 'Bed' else kind
    return [c for c in components(actor) if name(c).startswith(prefix)]


def local_mesh_values(component, spec):
    kind, label, origin, yaw, gameplay_label = spec
    values = props(component, MESH_PROPS)
    if kind == 'Lever':
        # Both native lever meshes use the same local geometry; visual root uses
        # the functional lever's yaw and floor position.
        return values
    values['relativeLocation'] = local_point(values['relativeLocation'], origin, yaw)
    values['relativeRotation']['yaw'] -= yaw
    return values


def bp_ref(kind):
    path = FOLDER + '/BP_HubVisual_' + kind
    return ref(path + '.BP_HubVisual_' + kind)


def roots(bp):
    return props(bp, ['placeholderRoot', 'collisionRoot', 'interactionAnchor'])


def add_mesh(parent, component_name, values, collision=False):
    component = call('actor.ActorTools.add_component', {
        'owner': parent, 'component_type': ref('/Script/Engine.StaticMeshComponent'),
        'name': component_name})['returnValue']
    setp(component, {
        'staticMesh': values['staticMesh'], 'overrideMaterials': values['overrideMaterials'],
        'relativeLocation': values['relativeLocation'],
        'relativeRotation': values['relativeRotation'],
        'relativeScale3D': values['relativeScale3D'], 'mobility': 'Movable',
        'bVisible': not collision, 'bHiddenInGame': collision,
        'castShadow': values['castShadow'] if not collision else False,
        'bGenerateOverlapEvents': False,
        'bodyInstance': {'collisionProfileName': 'BlockAll' if collision else 'NoCollision',
                         'collisionEnabled': 'QueryAndPhysics' if collision else 'NoCollision'}})
    return component


def build_one(kind):
    assert kind in KINDS
    assert call('scene.SceneTools.get_current_level', {})['returnValue'] == LEVEL
    spec = next(s for s in SPECS if s[0] == kind)
    source = source_meshes(spec)
    assert source, kind
    path = FOLDER + '/BP_HubVisual_' + kind
    assert not call('asset.AssetTools.exists', {'path': path})['returnValue'], path
    bp = call('blueprint.BlueprintTools.create', {
        'folder_path': FOLDER, 'asset_name': 'BP_HubVisual_' + kind,
        'asset_type': ref('/Script/DIGETHER.DigetherHubVisualActor')})['returnValue']
    collision_count = 0
    for index, component in enumerate(source):
        values = local_mesh_values(component, spec)
        root = roots(bp)
        add_mesh(root['placeholderRoot'], 'Blockout_' + str(index), values)
        if values['bodyInstance']['collisionEnabled'] != 'NoCollision':
            # An invisible copy retains the blockout's dedicated collision mesh;
            # replacing art never changes this mesh or its transform.
            root = roots(bp)
            add_mesh(root['collisionRoot'], 'Collision_' + str(index), values, True)
            collision_count += 1
    function_pose = transform(one(spec[4]))
    anchor = roots(bp)['interactionAnchor']
    setp(anchor, {'relativeLocation': local_point(function_pose['location'], spec[2], spec[3]),
                  'relativeRotation': {'pitch': function_pose['rotation']['pitch'],
                      'yaw': function_pose['rotation']['yaw'] - spec[3],
                      'roll': function_pose['rotation']['roll']}})
    call('blueprint.BlueprintTools.compile_blueprint', {'blueprint': bp, 'warnings_as_errors': False})
    assert call('asset.AssetTools.save_assets', {'asset_paths': [path]})['returnValue']
    return {'kind': kind, 'blueprint': bp, 'placeholder_count': len(source),
            'collision_count': collision_count, 'saved': True}


def wire_instances():
    assert call('scene.SceneTools.get_current_level', {})['returnValue'] == LEVEL
    result = []
    for kind, label, origin, yaw, gameplay_label in SPECS:
        visual_label = 'Hub_Visual_' + label
        found = call('scene.SceneTools.find_actors', {
            'root': None, 'name': visual_label, 'actor_type': None, 'tag': '',
            'bounds': None, 'collision_channels': []})['returnValue']
        assert not found, visual_label
        gameplay = one(gameplay_label)
        before = transform(gameplay)
        visual = call('scene.SceneTools.add_to_scene_from_class', {
            'actor_type': ref(bp_ref(kind)['refPath'] + '_C'), 'name': visual_label,
            'xform': {'location': dict(zip(['x', 'y', 'z'], origin)),
                      'rotation': {'pitch': 0, 'yaw': yaw, 'roll': 0},
                      'scale': {'x': 1, 'y': 1, 'z': 1}},
            'parent': None, 'snap_to_ground': False})['returnValue']
        setp(visual, {'gameplayActor': gameplay})
        call('scene.SceneTools.set_actor_folder', {'actor': visual, 'folder_path': 'Hub/Facilities'})
        if kind in ['Elevator', 'Lever']:
            child = call('actor.ActorTools.get_root_component', {'actor': visual})['returnValue']
            parent = call('actor.ActorTools.get_root_component', {'actor': gameplay})['returnValue']
        else:
            child = call('actor.ActorTools.get_root_component', {'actor': gameplay})['returnValue']
            parent = props(visual, ['interactionAnchor'])['interactionAnchor']
        assert call('actor.ActorTools.set_parent_component', {'component': child, 'parent': parent})['returnValue']
        assert close_transform(transform(gameplay), before), (gameplay, before, transform(gameplay))
        result.append({'label': visual_label, 'gameplay': gameplay, 'gameplay_transform': before})
    assert call('asset.AssetTools.save_assets', {'asset_paths': [LEVEL]})['returnValue']
    return {'instances': result, 'saved': True}


def close_number(left, right):
    if isinstance(left, dict):
        return all(close_number(left[key], right[key]) for key in left)
    return abs(left - right) < .01


def close_rotation(left, right):
    return all(abs((left[key] - right[key] + 180) % 360 - 180) < .01
               for key in ['pitch', 'yaw', 'roll'])


def close_transform(left, right):
    return (close_number(left['location'], right['location'])
            and close_number(left['scale'], right['scale'])
            and close_rotation(left['rotation'], right['rotation']))


def relative_transform(values):
    return {'location': values['relativeLocation'], 'rotation': values['relativeRotation'],
            'scale': values['relativeScale3D']}


def preset_transform(origin=(0, 0, 0), yaw=0):
    return {'location': dict(zip(['x', 'y', 'z'], origin)),
            'rotation': {'pitch': 0, 'yaw': yaw, 'roll': 0},
            'scale': {'x': 1, 'y': 1, 'z': 1}}


def compose_yaw(parent, child):
    # Hub parents have unit scale and yaw only; the leaf meshes may be pitched.
    # Refuse a changed authoring convention instead of silently losing shear.
    assert close_rotation(parent['rotation'], {'pitch': 0, 'yaw': parent['rotation']['yaw'], 'roll': 0}), parent
    assert close_number(parent['scale'], {'x': 1, 'y': 1, 'z': 1}), parent
    angle = math.radians(parent['rotation']['yaw'])
    point = child['location']
    return {'location': {
        'x': parent['location']['x'] + point['x'] * math.cos(angle) - point['y'] * math.sin(angle),
        'y': parent['location']['y'] + point['x'] * math.sin(angle) + point['y'] * math.cos(angle),
        'z': parent['location']['z'] + point['z']},
        'rotation': {'pitch': child['rotation']['pitch'],
                     'yaw': parent['rotation']['yaw'] + child['rotation']['yaw'],
                     'roll': child['rotation']['roll']},
        'scale': child['scale']}


def component_parent(component):
    parent = call('actor.ActorTools.get_parent_component', {'component': component}).get('returnValue')
    return None if empty_ref(parent) else parent


def empty_ref(value):
    return value is None or value in ('', 'None') or (
        isinstance(value, dict) and value.get('refPath') in (None, '', 'None'))


def component_world(component, cache, values=None):
    path = component['refPath']
    if path not in cache:
        local = relative_transform(values or props(component, [
            'relativeLocation', 'relativeRotation', 'relativeScale3D']))
        parent = component_parent(component)
        cache[path] = compose_yaw(component_world(parent, cache), local) if parent else local
    return cache[path]


def checked_properties(obj, required, optional=()):
    schema = json.loads(call('object.ObjectTools.list_properties', {'instance': obj})['returnValue'])
    assert set(required).issubset(schema), (obj, required, sorted(schema))
    return props(obj, list(required) + [key for key in optional if key in schema])


def mesh_snapshot(component):
    values = props(component, MESH_PROPS + ['bVisible', 'bHiddenInGame',
                                          'bGenerateOverlapEvents', 'mobility'])
    values['bodyInstance'] = {key: values['bodyInstance'][key]
                              for key in ['collisionEnabled', 'collisionProfileName']}
    return values


def exact_actors(label):
    found = call('scene.SceneTools.find_actors', {
        'root': None, 'name': label, 'actor_type': None, 'tag': '',
        'bounds': None, 'collision_channels': []})['returnValue']
    return [actor for actor in found if call('actor.ActorTools.get_label',
            {'actor': actor})['returnValue'] == label]


def capture_baseline():
    """Return a JSON-serializable snapshot before wiring or removing sources."""
    assert call('scene.SceneTools.get_current_level', {})['returnValue'] == LEVEL
    baseline = {'version': 1, 'level': LEVEL, 'facilities': {}, 'source_groups': {}}
    cache, consumed = {}, []
    for spec in SPECS:
        kind, label, origin, yaw, gameplay_label = spec
        gameplay = one(gameplay_label)
        if kind == 'Deposit':
            required, optional = ['stationId'], []
        elif kind == 'Lever':
            required, optional = ['leverKind', 'elevatorActor'], []
        elif kind == 'Elevator':
            required = ['boardingVolume']
            optional = ['doorThresholdVolume', 'doorOutwardDirection']
        else:
            required = ['facilityId', 'kind', 'interactionDistanceCm', 'restLocationOffset']
            optional = []
        function_values = checked_properties(gameplay, required, optional)
        volumes = {}
        for key in ['boardingVolume', 'doorThresholdVolume']:
            if not empty_ref(function_values.get(key)):
                volume = function_values[key]
                values = checked_properties(volume, ['relativeLocation', 'relativeRotation',
                    'relativeScale3D', 'boxExtent', 'bodyInstance'])
                values['bodyInstance'] = {field: values['bodyInstance'][field]
                    for field in ['collisionEnabled', 'collisionProfileName']}
                volumes[key] = {'component': volume, 'values': values}
        root = call('actor.ActorTools.get_root_component', {'actor': gameplay})['returnValue']
        entry = {'spec': json.loads(json.dumps(spec)), 'gameplay': gameplay,
                 'gameplay_transform': transform(gameplay), 'gameplay_properties': function_values,
                 'gameplay_parent': component_parent(root), 'volumes': volumes, 'meshes': []}
        for component in source_meshes(spec):
            values = mesh_snapshot(component)
            entry['meshes'].append({'component': component, 'values': values,
                                    'world_transform': component_world(component, cache, values)})
            consumed.append(component['refPath'])
        assert entry['meshes'], label
        baseline['facilities'][label] = entry
    assert len(consumed) == len(set(consumed)), 'A source mesh belongs to more than one facility'
    for label in ['Hub_Furniture', 'Hub_Altar']:
        actor = one(label)
        assert close_transform(transform(actor), preset_transform()), (label, transform(actor))
        paths = sorted(component['refPath'] for component in components(actor))
        assert set(paths).issubset(consumed), (label, sorted(set(paths) - set(consumed)))
        baseline['source_groups'][label] = {'actor': actor, 'components': paths}
    baseline['consumed_components'] = sorted(consumed)
    state = one('Hub_Gameplay_State')
    baseline['hub_state'] = {'actor': state,
                             'values': checked_properties(state, ['catalog', 'offeringLight'])}
    return baseline


def same_properties(left, right):
    if isinstance(left, dict):
        if set(left) == {'pitch', 'yaw', 'roll'}:
            return close_rotation(left, right)
        return isinstance(right, dict) and left.keys() == right.keys() and all(
            same_properties(left[key], right[key]) for key in left)
    if isinstance(left, list):
        return isinstance(right, list) and len(left) == len(right) and all(
            same_properties(a, b) for a, b in zip(left, right))
    if isinstance(left, (int, float)) and not isinstance(left, bool):
        return isinstance(right, (int, float)) and abs(left - right) < .01
    return left == right


def validate(baseline, retired=False):
    assert baseline['version'] == 1 and baseline['level'] == LEVEL
    assert call('scene.SceneTools.get_current_level', {})['returnValue'] == LEVEL
    assert set(baseline['facilities']) == {spec[1] for spec in SPECS}
    consumed = [mesh['component']['refPath'] for entry in baseline['facilities'].values()
                for mesh in entry['meshes']]
    assert len(consumed) == len(set(consumed))
    assert sorted(consumed) == baseline['consumed_components']
    for label, group in baseline['source_groups'].items():
        assert set(group['components']).issubset(consumed), label
        found = exact_actors(label)
        assert len(found) == (0 if retired else 1), (label, retired, found)
        if not retired:
            assert found[0] == group['actor'], label
            assert sorted(c['refPath'] for c in components(found[0])) == group['components'], label
    state = baseline['hub_state']
    assert props(state['actor'], list(state['values'])) == state['values'], 'Hub state links changed'
    result, cache = [], {}
    for spec in SPECS:
        kind, label, origin, yaw, gameplay_label = spec
        entry = baseline['facilities'][label]
        assert entry['spec'] == json.loads(json.dumps(spec)), label
        visual, gameplay = one('Hub_Visual_' + label), one(gameplay_label)
        assert gameplay == entry['gameplay'], label
        assert close_transform(transform(gameplay), entry['gameplay_transform']), (label, 'gameplay pose')
        expected_properties = entry['gameplay_properties']
        assert same_properties(props(gameplay, list(expected_properties)), expected_properties), (label, 'gameplay properties')
        for key, volume in entry['volumes'].items():
            actual = props(volume['component'], list(volume['values']))
            actual['bodyInstance'] = {field: actual['bodyInstance'][field]
                for field in ['collisionEnabled', 'collisionProfileName']}
            assert same_properties(actual, volume['values']), (label, key)
        assert close_transform(transform(visual), preset_transform(origin, yaw)), (label, 'visual pose')
        vprops = props(visual, ['sceneRoot', 'gameplayActor', 'interactionAnchor', 'replacementMesh',
                               'placeholderRoot', 'collisionRoot', 'artRoot', 'bUseCustomArt'])
        assert vprops['gameplayActor'] == gameplay and not vprops['bUseCustomArt'], label
        visual_root = call('actor.ActorTools.get_root_component', {'actor': visual})['returnValue']
        gameplay_root = call('actor.ActorTools.get_root_component', {'actor': gameplay})['returnValue']
        assert visual_root == vprops['sceneRoot'], label
        if kind in ['Elevator', 'Lever']:
            assert component_parent(visual_root) == gameplay_root, (label, 'visual attachment')
            assert component_parent(gameplay_root) == entry['gameplay_parent'], (label, 'gameplay attachment')
        else:
            assert component_parent(gameplay_root) == vprops['interactionAnchor'], (label, 'gameplay attachment')
            assert component_parent(visual_root) is None, (label, 'visual attachment')
        for key in ['artRoot', 'placeholderRoot', 'collisionRoot', 'interactionAnchor']:
            assert component_parent(vprops[key]) == visual_root, (label, key, 'parent')
            if key != 'interactionAnchor':
                values = props(vprops[key], ['relativeLocation', 'relativeRotation', 'relativeScale3D'])
                assert close_transform(relative_transform(values), preset_transform()), (label, key, 'transform')
        assert close_transform(component_world(vprops['interactionAnchor'], cache),
                               entry['gameplay_transform']), (label, 'interaction anchor')
        assert component_parent(vprops['replacementMesh']) == vprops['artRoot'], label
        assert empty_ref(props(vprops['replacementMesh'], ['staticMesh'])['staticMesh']), (label, 'custom mesh already set')
        meshes = {name(component): component for component in components(visual)}
        expected_names = {'ReplacementMesh'}
        collider_count = 0
        for index, source in enumerate(entry['meshes']):
            original = source['values']
            has_collision = original['bodyInstance']['collisionEnabled'] != 'NoCollision'
            for collision in ([False, True] if has_collision else [False]):
                component_name = ('Collision_' if collision else 'Blockout_') + str(index)
                expected_names.add(component_name)
                assert component_name in meshes, (label, component_name)
                component = meshes[component_name]
                actual = mesh_snapshot(component)
                assert component_parent(component) == vprops['collisionRoot' if collision else 'placeholderRoot'], (label, component_name, 'parent')
                assert close_transform(component_world(component, cache, actual),
                                       source['world_transform']), (label, component_name, 'world geometry')
                for key in ['staticMesh', 'overrideMaterials']:
                    assert actual[key] == original[key], (label, component_name, key)
                assert actual['bVisible'] == (False if collision else original['bVisible']), (label, component_name, 'visibility')
                assert actual['bHiddenInGame'] == (True if collision else original['bHiddenInGame']), (label, component_name, 'hidden')
                expected_body = original['bodyInstance'] if collision else {
                    'collisionEnabled': 'NoCollision', 'collisionProfileName': 'NoCollision'}
                assert actual['bodyInstance'] == expected_body, (label, component_name, 'collision')
                assert not actual['bGenerateOverlapEvents'] and actual['mobility'] == 'Movable', (label, component_name, 'mobility/overlap')
                assert actual['castShadow'] == (False if collision else original['castShadow']), (label, component_name, 'shadow')
                if collision:
                    collider_count += 1
            if not retired or kind == 'Lever':
                current = mesh_snapshot(source['component'])
                assert close_transform(component_world(source['component'], cache, current), source['world_transform']), (label, 'source geometry')
                if retired:
                    assert not current['bVisible'] and current['bHiddenInGame'], (label, 'native source visible')
                    assert current['bodyInstance']['collisionEnabled'] == 'NoCollision', label
                    for key in ['staticMesh', 'overrideMaterials']:
                        assert current[key] == original[key], (label, 'native source', key)
                else:
                    assert same_properties(current, original), (label, 'source changed')
        assert set(meshes) == expected_names, (label, 'unexpected mesh components', set(meshes) ^ expected_names)
        art_state = props(vprops['replacementMesh'], ['bodyInstance', 'bVisible', 'bHiddenInGame'])
        assert art_state['bodyInstance']['collisionEnabled'] == 'NoCollision', label
        assert not art_state['bVisible'] and art_state['bHiddenInGame'], (label, 'empty art visibility')
        result.append({'label': label, 'placeholder_count': len(entry['meshes']),
                       'collision_count': collider_count, 'gameplay_transform': transform(gameplay)})
    return {'passed': True, 'facilities': result, 'retired': retired}


def retire_sources(baseline):
    validate(baseline)
    for group in ['Hub_Furniture', 'Hub_Altar']:
        actor = one(group)
        assert call('scene.SceneTools.remove_from_scene', {'actor': actor})['returnValue']
    for spec in SPECS:
        if spec[0] == 'Lever':
            for mesh in components(one(spec[4])):
                setp(mesh, {'bVisible': False, 'bHiddenInGame': True,
                            'bodyInstance': {'collisionProfileName': 'NoCollision',
                                             'collisionEnabled': 'NoCollision'}})
    result = validate(baseline, retired=True)
    assert call('asset.AssetTools.save_assets', {'asset_paths': [LEVEL]})['returnValue']
    result['saved'] = True
    return result
