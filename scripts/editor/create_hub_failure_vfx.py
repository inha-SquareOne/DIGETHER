"""Connect the approved primitive flood/suction preview to the existing well BP.

The root operator runs this in the live Unreal MCP sandbox after the native
visual component is built and the well package is backed up. Engine shapes and
existing Hub materials are reused, with a two-sided water instance copy for the
flood preview; no import, damage or gameplay timing changes.
Geometry and lighting values below are provisional presentation values.

MCP SCS 템플릿 변경은 기배치 인스턴스에 전파되지 않는다. 기존 배치가 있으면
참조와 원래 native 배치값을 확인한 뒤 root가 별도로 재배치·검증해야 한다.
이 스크립트는 기존 인스턴스의 자동 동기화나 교체를 수행하지 않는다.
"""
import json

WF_BP = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
WF_OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
WF_ACTOR = 'editor_toolset.toolsets.actor.ActorTools.'
WF_ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
WF_SCENE = 'editor_toolset.toolsets.scene.SceneTools.'
WF_PATH = '/Game/Game/Blueprints/BP_DigetherHubWell'
WF_FLOOD_MATERIAL = '/Game/Game/Environment/Hub/Materials/MI_HubFloodPreview'
WF_CLASS = '/Script/DIGETHER.DigetherHubWellVisualComponent'
WF_NAMES = ['FloodSurface', 'SuctionShape', 'Glow', 'WellVisual']


def wf_ref(path):
    return {'refPath': path}


def wf_tool(tool_name, **arguments):
    result = execute_tool(tool_name, json.dumps(arguments))
    assert 'error' not in result, (tool_name, result)
    return result


def wf_names(obj):
    return json.loads(wf_tool(WF_OBJ + 'list_properties', instance=obj)['returnValue'])


def wf_getp(obj, names):
    available = wf_names(obj)
    assert all(name in available for name in names), (obj, names)
    return json.loads(wf_tool(WF_OBJ + 'get_properties', instance=obj,
                              properties=names)['returnValue'])


def wf_setp(obj, values):
    wf_getp(obj, list(values))
    assert wf_tool(WF_OBJ + 'set_properties', instance=obj,
                   values=json.dumps(values))['returnValue'], (obj, list(values))


def wf_asset(path):
    assert wf_tool(WF_ASSET + 'exists', path=path)['returnValue'], path
    return wf_ref(path + '.' + path.split('/')[-1])


def wf_info(node):
    return wf_tool(WF_BP + 'get_node_infos', nodes=[node])['returnValue'][0]


def wf_pin(node, name, is_input):
    matches = [pin for pin in node['input_pins' if is_input else 'output_pins']
               if pin['name'] == name]
    assert len(matches) == 1, (node['type_id'], name)
    return matches[0]


def wf_type(graph, method, inputs, outputs):
    candidates = wf_tool(WF_BP + 'find_node_types', graph=graph,
        type_id_filter=method, context_pins=[])['returnValue']
    matches = list(dict.fromkeys(candidate for candidate in candidates
        if candidate.rsplit('|', 1)[-1] == method))
    assert len(matches) == 1, (method, candidates)
    pins = wf_tool(WF_BP + 'get_node_type_pins', graph=graph,
                   type_id=matches[0])['returnValue']
    for name in inputs:
        wf_pin(pins, name, True)
    for name in outputs:
        wf_pin(pins, name, False)
    return matches[0]


def wf_make(graph, type_id, x, y, declaring_class=None):
    arguments = {'graph': graph, 'type_id': type_id, 'pos': {'x': x, 'y': y}}
    if declaring_class is not None:
        arguments['declaring_class'] = declaring_class
    return wf_info(wf_tool(WF_BP + 'create_node', **arguments)['returnValue'])


def wf_link(source, output, destination, input_name):
    wf_tool(WF_BP + 'connect_pins',
            output_pin=wf_pin(source, output, False)['pin_id'],
            input_pin=wf_pin(destination, input_name, True)['pin_id'])


def wf_native_snapshot(bp):
    values = wf_getp(bp, ['sceneRoot', 'waitingPoint', 'suctionRadiusCm',
                         'waitingSpacingCm'])
    transforms = {}
    for name in ['sceneRoot', 'waitingPoint']:
        transforms[name] = wf_getp(values[name], ['relativeLocation',
            'relativeRotation', 'relativeScale3D', 'mobility'])
    return {'defaults': values, 'transforms': transforms}


def wf_new_component(bp, name, component_class):
    # Adding a component compiles the BP; retain the returned SCS template ref.
    component = wf_tool(WF_ACTOR + 'add_component', owner=bp,
        component_type=wf_ref(component_class), name=name)['returnValue']
    assert isinstance(component, dict) and component.get('refPath'), name
    return component


def wf_mesh(component, mesh, material, location, scale, rotation):
    wf_setp(component, {'staticMesh': mesh, 'overrideMaterials': [material],
        'relativeLocation': location, 'relativeScale3D': scale,
        'relativeRotation': rotation, 'mobility': 'Movable',
        'bVisible': False, 'bHiddenInGame': False, 'castShadow': False,
        'bGenerateOverlapEvents': False, 'bCanEverAffectNavigation': False,
        'bodyInstance': {'collisionProfileName': 'NoCollision',
                         'collisionEnabled': 'NoCollision'}})


def run():
    assert not wf_tool('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    native_classes = wf_tool(WF_OBJ + 'search_subclasses',
        base_class=wf_ref('/Script/Engine.ActorComponent'),
        class_name='DigetherHubWellVisualComponent')['returnValue']
    assert wf_ref(WF_CLASS) in native_classes, 'Build and load the native visual component first'
    bp = wf_asset(WF_PATH)
    native_before = wf_native_snapshot(bp)
    level = wf_tool(WF_SCENE + 'get_current_level')['returnValue']
    map_dirty_before = wf_tool(WF_ASSET + 'is_dirty', asset_path=level)['returnValue'] if level else None
    available = wf_names(bp)
    for name in WF_NAMES:
        assert not any(key.lower() == name.lower() for key in available), 'Existing component: ' + name
    cdo = wf_tool(WF_BP + 'get_default_object', blueprint=bp)['returnValue']
    native_components = wf_tool(WF_ACTOR + 'get_components', actor=cdo)['returnValue']
    for component in native_components:
        leaf = component['refPath'].rsplit(':', 1)[-1].rsplit('.', 1)[-1]
        assert leaf not in WF_NAMES, 'Existing component: ' + leaf

    graph = wf_tool(WF_BP + 'get_graph', blueprint=bp,
                    graph_name='EventGraph')['returnValue']
    # Resolve the real callable IDs and signatures before modifying the BP.
    methods = {
        'StartFlood': wf_type(graph, 'StartFlood',
            ['execute', 'self', 'FloodSeconds', 'FailMotionSeconds',
             'FloodSurface', 'SuctionShape', 'Glow'], ['then', 'ReturnValue']),
        'StartSuction': wf_type(graph, 'StartSuction',
            ['execute', 'self'], ['then', 'ReturnValue']),
        'StopVisuals': wf_type(graph, 'StopVisuals', ['execute', 'self'], ['then'])}
    events = wf_tool(WF_BP + 'list_events', blueprint=bp)['returnValue']
    event_names = ['BP_OnFloodStart', 'BP_OnSuctionStart', 'BP_OnFailureEventEnd']
    for name in event_names:
        matches = [event for event in events if event['name'] == name]
        assert len(matches) == 1 and not matches[0]['bIsImplemented'], name
    water_source_path = '/Game/Game/Environment/Hub/Materials/MI_HubWater'
    wf_asset(water_source_path)
    teal = wf_asset('/Game/Game/Environment/Hub/Materials/MI_HubTealGlow')
    plane = wf_asset('/Engine/BasicShapes/Plane')
    cone = wf_asset('/Engine/BasicShapes/Cone')
    assert not wf_tool(WF_ASSET + 'exists', path=WF_FLOOD_MATERIAL)['returnValue'], WF_FLOOD_MATERIAL
    assert wf_tool(WF_ASSET + 'duplicate', path=water_source_path,
                   new_path=WF_FLOOD_MATERIAL)['returnValue'], WF_FLOOD_MATERIAL
    water = wf_asset(WF_FLOOD_MATERIAL)
    # Keep the shared Hub water unchanged; the rising surface is visible below.
    wf_setp(water, {'basePropertyOverrides': {
        'bOverride_TwoSided': True, 'twoSided': True}})

    components = {}
    components['FloodSurface'] = wf_new_component(bp, 'FloodSurface', '/Script/Engine.StaticMeshComponent')
    wf_mesh(components['FloodSurface'], plane, water,
        {'x': 0, 'y': 0, 'z': -35}, {'x': 3, 'y': 3, 'z': 1},
        {'pitch': 0, 'yaw': 0, 'roll': 0})
    components['SuctionShape'] = wf_new_component(bp, 'SuctionShape', '/Script/Engine.StaticMeshComponent')
    wf_mesh(components['SuctionShape'], cone, teal,
        {'x': 0, 'y': 0, 'z': -100}, {'x': 6, 'y': 6, 'z': 4},
        {'pitch': 0, 'yaw': 0, 'roll': 180})
    components['Glow'] = wf_new_component(bp, 'Glow', '/Script/Engine.PointLightComponent')
    # Match the existing Hub's low-intensity, noninverse-square trial lighting.
    wf_setp(components['Glow'], {'relativeLocation': {'x': 0, 'y': 0, 'z': 100},
        'mobility': 'Movable', 'bVisible': False, 'bHiddenInGame': False,
        'bCanEverAffectNavigation': False, 'castShadows': False,
        'lightColor': {'r': .05, 'g': 1, 'b': .78, 'a': 1},
        'intensityUnits': 'Unitless', 'bUseInverseSquaredFalloff': False,
        'intensity': 2.5, 'attenuationRadius': 1200, 'lightFalloffExponent': 2})
    components['WellVisual'] = wf_new_component(bp, 'WellVisual', WF_CLASS)
    # This BP's light is Unitless, matching the existing Hub rather than candela defaults.
    wf_setp(components['WellVisual'], {'floodGlowIntensity': 4.0, 'suctionGlowIntensity': 8.0})

    # add_event also invalidates the action cache after add_component compilation.
    event_nodes = {}
    for index, name in enumerate(event_names):
        event = wf_info(wf_tool(WF_BP + 'add_event', blueprint=bp,
            event_name=name, position={'x': 0, 'y': index * 400})['returnValue'])
        assert not wf_pin(event, 'then', False)['connected_pins'], name
        event_nodes[name] = event
    getters = {}
    for index, name in enumerate(WF_NAMES):
        getter_type = wf_type(graph, 'Get' + name, [], [])
        getter = wf_make(graph, getter_type, 180, -300 - index * 120)
        assert len(getter['output_pins']) == 1, getter
        getters[name] = getter

    call_nodes = {}
    for index, (event_name, method) in enumerate(zip(event_names,
            ['StartFlood', 'StartSuction', 'StopVisuals'])):
        node = wf_make(graph, methods[method], 600, index * 400,
                       declaring_class=wf_ref(WF_CLASS))
        wf_link(event_nodes[event_name], 'then', node, 'execute')
        visual = getters['WellVisual']
        wf_link(visual, visual['output_pins'][0]['name'], node, 'self')
        call_nodes[method] = node
    flood = call_nodes['StartFlood']
    for name in ['FloodSeconds', 'FailMotionSeconds']:
        wf_link(event_nodes['BP_OnFloodStart'], name, flood, name)
    for name in ['FloodSurface', 'SuctionShape', 'Glow']:
        getter = getters[name]
        wf_link(getter, getter['output_pins'][0]['name'], flood, name)

    wf_tool(WF_BP + 'compile_blueprint', blueprint=bp, warnings_as_errors=True)
    assert wf_native_snapshot(bp) == native_before, 'Native well defaults changed'
    safety = {}
    for name in ['FloodSurface', 'SuctionShape']:
        values = wf_getp(components[name], ['staticMesh', 'overrideMaterials',
            'mobility', 'bVisible', 'bHiddenInGame', 'castShadow',
            'bGenerateOverlapEvents', 'bCanEverAffectNavigation', 'bodyInstance'])
        assert values['bodyInstance']['collisionEnabled'] == 'NoCollision', name
        assert not values['bGenerateOverlapEvents'] and not values['bCanEverAffectNavigation'], name
        assert not values['bVisible'] and not values['castShadow'], name
        safety[name] = values
    overrides = wf_getp(water, ['basePropertyOverrides'])['basePropertyOverrides']
    assert overrides['bOverride_TwoSided'] and overrides['twoSided']
    assert wf_tool(WF_ASSET + 'save_assets', asset_paths=[WF_PATH, WF_FLOOD_MATERIAL])['returnValue']
    assert not wf_tool(WF_ASSET + 'is_dirty', asset_path=WF_PATH)['returnValue']
    assert not wf_tool(WF_ASSET + 'is_dirty', asset_path=WF_FLOOD_MATERIAL)['returnValue']
    return {'saved': WF_PATH, 'components': components,
        'existingInstanceValidationRequired': True,
        'existingInstanceNote': 'MCP SCS 템플릿 변경은 기배치 인스턴스에 전파되지 않음. 기존 배치의 참조·원래 native 배치값 확인 후 root 별도 재배치·검증 필요',
        'savedMaterials': [WF_FLOOD_MATERIAL], 'floodMaterialOverrides': overrides,
        'events': {name: node['node'] for name, node in event_nodes.items()},
        'callNodes': {name: node['node'] for name, node in call_nodes.items()},
        'nativeDefaultsPreserved': native_before, 'meshSafety': safety,
        'mapDirtyBefore': map_dirty_before,
        'mapDirtyAfter': wf_tool(WF_ASSET + 'is_dirty', asset_path=level)['returnValue'] if level else None,
        'presentationStatus': '기본 도형·기존 재질 시험 연출, 실제 카메라·취소·중첩·재접속 검증 대기'}
