"""Connect existing gameplay C++ contracts through live Unreal MCP tools.

The root editor operator owns the target BP/WBP packages and L_Hub while this
script runs. Back up the packages and inspect the registered schemas first.
MCP SCS template edits do not propagate to actors already placed in a map.
After adding presentation components, validate live instances and synchronize
only this operation's new properties before saving the map.
"""
import json

BP = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
SCENE = 'editor_toolset.toolsets.scene.SceneTools.'
ACTOR = 'editor_toolset.toolsets.actor.ActorTools.'
UMG = 'UMGToolSet.UMGToolSet.'


def ref(path):
    return {'refPath': path}


def tool(tool_name, **arguments):
    return execute_tool(tool_name, json.dumps(arguments))


def getp(obj, names):
    return json.loads(tool(OBJ + 'get_properties', instance=obj, properties=names)['returnValue'])


def setp(obj, values):
    available = json.loads(tool(OBJ + 'list_properties', instance=obj)['returnValue'])
    assert all(name in available for name in values), (obj, list(values))
    getp(obj, list(values))
    assert tool(OBJ + 'set_properties', instance=obj, values=json.dumps(values))['returnValue']


def asset(path):
    assert tool(ASSET + 'exists', path=path)['returnValue'], path
    return ref(path + '.' + path.split('/')[-1])


def widgets(bp):
    return tool(UMG + 'GetWidgets', widgetBlueprint=bp)['returnValue']


def find_actors(cls):
    return tool(SCENE + 'find_actors', name='', actor_type=ref('/Script/DIGETHER.' + cls),
                tag='', collision_channels=[])['returnValue']


def add_widget(bp, cls, name, parent=None):
    info = tool(UMG + 'AddWidget', widgetBlueprint=bp, widgetClass=ref('/Script/UMG.' + cls),
                widgetDisplayName=name, parentWidget=parent, childIndex=-1)['returnValue']
    if name.startswith(('Text_', 'Button_')) or name == 'Panel_Scroll':
        tool(UMG + 'ToggleWidgetAsVariable', widgetBlueprint=bp, widget=info['widget'], bIsVariable=True)
    return info


def canvas_slot(info, x, y, width, height, anchor=(0, 0), alignment=(0, 0)):
    setp(info['slot'], {'layoutData': {'offsets': {'left': x, 'top': y, 'right': width, 'bottom': height},
         'anchors': {'minimum': {'x': anchor[0], 'y': anchor[1]},
                     'maximum': {'x': anchor[0], 'y': anchor[1]}},
         'alignment': {'x': alignment[0], 'y': alignment[1]}}})


def add_text(bp, name, parent, label, size=28):
    info = add_widget(bp, 'TextBlock', name, parent)
    setp(info['widget'], {'text': label, 'font': {'size': size}, 'justification': 'Center',
         'autoWrapText': True, 'colorAndOpacity': {'specifiedColor': {'r': .9, 'g': .82, 'b': .64, 'a': 1},
                                               'colorUseRule': 'UseColor_Specified'}})
    return info


def create_pour_input():
    source = '/Game/Game/Input/'
    action_path = source + 'Actions/IA_Pour'
    context_path = source + 'IMC_Pour'
    for old, new in [(source + 'Actions/IA_HubInteract', action_path),
                     (source + 'IMC_HubInteract', context_path)]:
        assert not tool(ASSET + 'exists', path=new)['returnValue'], 'Refusing to overwrite ' + new
        assert tool(ASSET + 'duplicate', path=old, new_path=new)['returnValue']
    action = asset(action_path)
    context = asset(context_path)
    assert getp(action, ['valueType'])['valueType'] == 'Boolean'
    mappings = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']['mappings']
    assert len(mappings) == 1 and not mappings[0]['triggers']
    mappings[0]['action'] = action
    mappings[0]['key'] = 'RightMouseButton'
    setp(context, {'defaultKeyMappings': {'mappings': mappings}})
    return action, context


def create_credits():
    path = '/Game/Game/UI/Hub/WBP_HubGameOverCredits'
    assert not tool(ASSET + 'exists', path=path)['returnValue'], 'Refusing to overwrite ' + path
    bp = tool(UMG + 'CreateWidgetBlueprint', folderPath='/Game/Game/UI/Hub',
              assetName='WBP_HubGameOverCredits', parentClass=ref('/Script/DIGETHER.DigetherGameOverCreditsWidget'))['returnValue']
    root = add_widget(bp, 'CanvasPanel', 'Canvas_Root')['widget']
    background = add_widget(bp, 'Border', 'Border_Background', root)
    setp(background['widget'], {'brushColor': {'r': .004, 'g': .006, 'b': .009, 'a': 1},
                               'visibility': 'HitTestInvisible'})
    setp(background['slot'], {'layoutData': {'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}},
                                           'offsets': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0}}})
    scroll = add_widget(bp, 'CanvasPanel', 'Panel_Scroll', root)
    canvas_slot(scroll, 0, 0, 1000, 1500, (.5, 0), (.5, 0))
    for name, label, y, height, size in [
        ('Text_Period', '합산 기간', 20, 70, 32),
        ('Text_TotalEarned', '플레이어 전체 획득 총액', 100, 100, 30),
        ('Text_EarnedBlock', '획득 금액', 280, 310, 28),
        ('Text_HiddenBlock', '히든 슬롯으로 얻은 금액', 690, 310, 28),
        ('Text_DeathLossBlock', '사망으로 잃은 금액', 1100, 310, 28)]:
        line = add_text(bp, name, scroll['widget'], label, size)
        canvas_slot(line, 30, y, 940, height)
    button = add_widget(bp, 'Button', 'Button_FastForward', root)
    canvas_slot(button, -40, -40, 300, 58, (1, 1), (1, 1))
    setp(button['widget'], {'backgroundColor': {'r': .12, 'g': .23, 'b': .23, 'a': 1}})
    fast_forward_text = add_text(bp, 'Text_FastForward', button['widget'], '빨리감기', 22)
    setp(fast_forward_text['widget'], {'autoWrapText': False})
    assert tool(UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp)['returnValue']
    return bp


def connect_controller(bp, action, context, credits):
    # Preserve the existing screen and interaction choices; complete only missing references.
    current = getp(bp, ['interactAction', 'interactMappingContext', 'runResultWidgetClass'])
    values = {'pourAction': action, 'pourMappingContext': context,
              'gameOverCreditsWidgetClass': ref(credits['refPath'] + '_C')}
    if current['interactAction'] == 'None':
        values['interactAction'] = asset('/Game/Game/Input/Actions/IA_HubInteract')
        values['interactMappingContext'] = asset('/Game/Game/Input/IMC_HubInteract')
    if current['runResultWidgetClass'] == 'None':
        values['runResultWidgetClass'] = ref('/Game/Game/UI/Hub/WBP_HubRunResult.WBP_HubRunResult_C')
    setp(bp, values)
    tool(BP + 'compile_blueprint', blueprint=bp, warnings_as_errors=True)
    cdo = tool(BP + 'get_default_object', blueprint=bp)['returnValue']
    components = tool(ACTOR + 'get_components', actor=cdo)['returnValue']
    paths = [component['refPath'] for component in components]
    for name in ['HubInteractionComponent', 'PlayerScreenComponent']:
        assert len([path for path in paths if path.endswith(':' + name)]) == 1, paths
    return {'defaults': getp(bp, list(values)), 'components': paths}


def connect_core():
    assert not tool('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    action, context = create_pour_input()
    credits = create_credits()
    return finish_core(action, context, credits)


def finish_core(action, context, credits):
    controllers = {}
    for name in ['BP_DigetherHubPlayerController', 'BP_DigetherGameplayPlayerController_Test']:
        controllers[name] = connect_controller(asset('/Game/Game/Blueprints/' + name), action, context, credits)
    saved = [action['refPath'], context['refPath'], credits['refPath']]
    saved += ['/Game/Game/Blueprints/' + name for name in controllers]
    assert tool(ASSET + 'save_assets', asset_paths=saved)['returnValue']
    return {'saved': saved, 'controllers': controllers}


def graph_for(bp):
    return tool(BP + 'get_graph', blueprint=bp, graph_name='EventGraph')['returnValue']


def node_type(graph, search, exact=None):
    matches = tool(BP + 'find_node_types', graph=graph, type_id_filter=search, context_pins=[])['returnValue']
    matches = list(dict.fromkeys(matches))
    if exact is not None:
        matches = [item for item in matches if item == exact]
    assert len(matches) == 1, (search, matches)
    # Read the pins even when the method is already familiar; IDs depend on editor locale.
    tool(BP + 'get_node_type_pins', graph=graph, type_id=matches[0])
    return matches[0]


def node_info(node):
    return tool(BP + 'get_node_infos', nodes=[node])['returnValue'][0]


def make_node(graph, type_id, x, y):
    return node_info(tool(BP + 'create_node', graph=graph, type_id=type_id,
                          pos={'x': x, 'y': y})['returnValue'])


def event_node(bp, name, x, y):
    info = node_info(tool(BP + 'add_event', blueprint=bp, event_name=name,
                          position={'x': x, 'y': y})['returnValue'])
    assert not pin_info(info, 'then', False)['connected_pins'], 'Existing event chain must be preserved'
    return info


def pin_info(node, name, is_input):
    matches = [pin for pin in node['input_pins' if is_input else 'output_pins'] if pin['name'] == name]
    assert len(matches) == 1, (node['type_id'], name)
    return matches[0]


def link(output, output_name, target, input_name):
    tool(BP + 'connect_pins', output_pin=pin_info(output, output_name, False)['pin_id'],
         input_pin=pin_info(target, input_name, True)['pin_id'])


def value(node, name, text):
    tool(BP + 'set_pin_value', pin=pin_info(node, name, True)['pin_id'], value=str(text))


def branch_node(graph, source, source_pin, condition, condition_pin, x, y):
    branch = make_node(graph, node_type(graph, 'Branch', 'Utilities|FlowControl|Branch'), x, y)
    link(source, source_pin, branch, 'execute')
    link(condition, condition_pin, branch, 'Condition')
    return branch


def compile_bp(bp):
    tool(BP + 'compile_blueprint', blueprint=bp, warnings_as_errors=True)


def connect_camera_fades():
    result = {}
    for name in ['BP_DigetherHubPlayerController', 'BP_DigetherGameplayPlayerController_Test']:
        bp = asset('/Game/Game/Blueprints/' + name)
        graph = graph_for(bp)
        fade = node_type(graph, 'StartCameraFade')
        manual_fade = node_type(graph, 'SetManualCameraFade')
        stop_fade = node_type(graph, 'StopCameraFade')
        camera = node_type(graph, 'GetPlayerCameraManager', 'Variables|플레이어컨트롤러|GetPlayerCameraManager')
        summoned = event_node(bp, 'ReceiveChainSummoned', 0, 0)
        manager = make_node(graph, camera, 180, -140)
        summon_fade = make_node(graph, fade, 420, 0)
        link(summoned, 'then', summon_fade, 'execute')
        link(manager, 'PlayerCameraManager', summon_fade, 'self')
        link(summoned, 'SummonSeconds', summon_fade, 'Duration')
        value(summon_fade, 'FromAlpha', '1.0')
        value(summon_fade, 'ToAlpha', '0.0')
        absorbed = event_node(bp, 'ReceiveGameOverAbsorbed', 0, 400)
        branch = branch_node(graph, absorbed, 'then', absorbed, 'bAbsorbed', 250, 400)
        for output, type_id, y in [('then', manual_fade, 400), ('else', stop_fade, 700)]:
            absorb_fade = make_node(graph, type_id, 520, y)
            link(branch, output, absorb_fade, 'execute')
            link(manager, 'PlayerCameraManager', absorb_fade, 'self')
            if output == 'then':
                value(absorb_fade, 'InFadeAmount', '1.0')
        compile_bp(bp)
        result[name] = {'chainEvent': summoned['node'], 'absorbedEvent': absorbed['node']}
    return result


def connect_credits_input():
    bp = asset('/Game/Game/UI/Hub/WBP_HubGameOverCredits')
    graph = graph_for(bp)
    assert not tool(BP + 'read_graph_dsl', graph=graph)['returnValue'].strip()
    for name in ['bCreditsInputApplied', 'bCreditsPreviousCursor']:
        tool(BP + 'add_variable', blueprint=bp, name=name, type_name='bool')
    tool(BP + 'compile_blueprint', blueprint=bp, warnings_as_errors=True)
    owner = node_type(graph, 'GetOwningPlayer', '위젯|GetOwningPlayer')
    host = node_type(graph, 'IsLocalHost')
    show_cursor = node_type(graph, 'SetShowMouseCursor')
    get_cursor = node_type(graph, 'GetShowMouseCursor')
    game_ui = node_type(graph, 'SetInputModeGameAndUI')
    game = node_type(graph, 'SetInputModeGameOnly')
    get_applied = node_type(graph, 'GetCreditsInputApplied')
    set_applied = node_type(graph, 'SetCreditsInputApplied')
    get_previous = node_type(graph, 'GetCreditsPreviousCursor')
    set_previous = node_type(graph, 'SetCreditsPreviousCursor')
    player = make_node(graph, owner, 0, -250)
    local_host = make_node(graph, host, 0, -100)
    applied = make_node(graph, get_applied, 200, -100)
    shown = event_node(bp, 'BP_OnCreditsShown', 0, 100)
    host_branch = branch_node(graph, shown, 'then', local_host, 'ReturnValue', 220, 100)
    applied_branch = branch_node(graph, host_branch, 'then', applied, 'bCreditsInputApplied', 440, 100)
    previous_cursor = make_node(graph, get_cursor, 600, -120)
    link(player, 'ReturnValue', previous_cursor, 'self')
    remember = make_node(graph, set_previous, 680, 100)
    link(applied_branch, 'else', remember, 'execute')
    link(previous_cursor, 'bShowMouseCursor', remember, 'bCreditsPreviousCursor')
    cursor_on = make_node(graph, show_cursor, 940, 100)
    link(remember, 'then', cursor_on, 'execute')
    link(player, 'ReturnValue', cursor_on, 'self')
    value(cursor_on, 'bShowMouseCursor', 'true')
    ui_mode = make_node(graph, game_ui, 1200, 100)
    link(cursor_on, 'then', ui_mode, 'execute')
    link(player, 'ReturnValue', ui_mode, 'PlayerController')
    value(ui_mode, 'bHideCursorDuringCapture', 'false')
    value(ui_mode, 'bFlushInput', 'true')
    mark = make_node(graph, set_applied, 1500, 100)
    link(ui_mode, 'then', mark, 'execute')
    value(mark, 'bCreditsInputApplied', 'true')
    for index, event_name in enumerate(['BP_OnCreditsHidden', 'Destruct']):
        y = 650 + index * 650
        event = event_node(bp, event_name, 0, y)
        restore_branch = branch_node(graph, event, 'then', applied, 'bCreditsInputApplied', 220, y)
        previous = make_node(graph, get_previous, 420, y - 130)
        restore_cursor = make_node(graph, show_cursor, 520, y)
        link(restore_branch, 'then', restore_cursor, 'execute')
        link(player, 'ReturnValue', restore_cursor, 'self')
        link(previous, 'bCreditsPreviousCursor', restore_cursor, 'bShowMouseCursor')
        restore_mode = make_node(graph, game, 820, y)
        link(restore_cursor, 'then', restore_mode, 'execute')
        link(player, 'ReturnValue', restore_mode, 'PlayerController')
        value(restore_mode, 'bFlushInput', 'true')
        clear = make_node(graph, set_applied, 1100, y)
        link(restore_mode, 'then', clear, 'execute')
        value(clear, 'bCreditsInputApplied', 'false')
    compile_bp(bp)
    return {'shown': shown['node'], 'restoration': ['BP_OnCreditsHidden', 'Destruct']}


def connect_payment_signal(light=None):
    bp = asset('/Game/Game/Blueprints/BP_DigetherDepositStation_Test')
    graph = graph_for(bp)
    assert not tool(BP + 'read_graph_dsl', graph=graph)['returnValue'].strip()
    if light is None:
        light = tool(ACTOR + 'add_component', owner=bp,
                     component_type=ref('/Script/Engine.PointLightComponent'), name='PaymentSignalLight')['returnValue']
    # Temporary presentation values; the public enum is the only economic input.
    setp(light, {'mobility': 'Movable', 'relativeLocation': {'x': 0, 'y': 0, 'z': 145},
                 'intensity': 300, 'attenuationRadius': 180, 'castShadows': False, 'bVisible': False})
    tool(BP + 'compile_blueprint', blueprint=bp, warnings_as_errors=True)
    event = event_node(bp, 'ReceivePaymentSignalChanged', 0, 0)
    getter = node_type(graph, 'GetPaymentSignalLight')
    visibility = node_type(graph, 'SetVisibility', '렌더링|SetVisibility')
    color = node_type(graph, 'SetLightColor', '렌더링|컴포넌트|라이트|SetLightColor')
    switch = node_type(graph, 'SwitchonEDigetherPaymentSignal')
    signal_switch = make_node(graph, switch, 220, 0)
    link(event, 'then', signal_switch, 'execute')
    link(event, 'Signal', signal_switch, 'Selection')
    light_getter = make_node(graph, getter, 220, -160)
    light_pin = light_getter['output_pins'][0]['name']
    for index, (name, rgba) in enumerate([('None', None), ('Red', '(R=1,G=0.025,B=0.01,A=1)'),
                       ('Orange', '(R=1,G=0.3,B=0.015,A=1)'),
                       ('Green', '(R=0.02,G=1,B=0.12,A=1)')]):
        visible = make_node(graph, visibility, 500, index * 300)
        link(signal_switch, name, visible, 'execute')
        link(light_getter, light_pin, visible, 'self')
        value(visible, 'bNewVisibility', 'false' if name == 'None' else 'true')
        if rgba:
            color_node = make_node(graph, color, 780, index * 300)
            link(visible, 'then', color_node, 'execute')
            link(light_getter, light_pin, color_node, 'self')
            value(color_node, 'NewLightColor', rgba)
            value(color_node, 'bSRGB', 'false')
    compile_bp(bp)
    return {'signalEvent': event['node'], 'light': light}


def connect_presentation():
    assert not tool('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    result = {'camera': connect_camera_fades(), 'credits': connect_credits_input(),
              'payment': connect_payment_signal()}
    saved = ['/Game/Game/Blueprints/' + name for name in result['camera']]
    saved += ['/Game/Game/UI/Hub/WBP_HubGameOverCredits', '/Game/Game/Blueprints/BP_DigetherDepositStation_Test']
    assert tool(ASSET + 'save_assets', asset_paths=saved)['returnValue']
    result['saved'] = saved
    return result


def connect_hub_map():
    assert not tool('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    level = '/Game/Game/Maps/L_Hub'
    assert tool(SCENE + 'get_current_level')['returnValue'] == level
    assert not find_actors('DigetherHubWellActor'), 'Well already placed'
    levers = find_actors('DigetherElevatorLeverActor')
    lever_state = [(actor, getp(actor, ['leverKind', 'placement', 'elevatorActor'])) for actor in levers]
    assert len(lever_state) == 2
    assert len([state for actor, state in lever_state if state['leverKind'] == 'Departure']) == 1
    assert len([state for actor, state in lever_state if state['leverKind'] == 'Ascent' and state['placement'] == 'Inside']) == 1
    elevator = lever_state[0][1]['elevatorActor']
    assert all(state['elevatorActor'] == elevator for actor, state in lever_state)
    baseline = {actor['refPath']: tool(ACTOR + 'get_actor_transform', actor=actor)['returnValue']
                for actor in levers + [elevator] + find_actors('DigetherHubFacilityActor')}
    well_path = '/Game/Game/Blueprints/BP_DigetherHubWell'
    assert not tool(ASSET + 'exists', path=well_path)['returnValue']
    well_bp = tool(BP + 'create', folder_path='/Game/Game/Blueprints', asset_name='BP_DigetherHubWell',
                   asset_type=ref('/Script/DIGETHER.DigetherHubWellActor'))['returnValue']
    compile_bp(well_bp)
    # Align the existing pool art; the native waiting offset and tuning stay unchanged.
    well = tool(SCENE + 'add_to_scene_from_class', actor_type=ref(well_bp['refPath'] + '_C'),
                name='Hub_GameOverWell', xform={'location': {'x': 0, 'y': -1060, 'z': 75}},
                snap_to_ground=False)['returnValue']
    outside = tool(SCENE + 'add_to_scene_from_class', actor_type=ref('/Script/DIGETHER.DigetherElevatorLeverActor'),
                   name='Hub_AscentLever_Outside', xform={'location': {'x': 320, 'y': 430, 'z': 0},
                   'rotation': {'pitch': 0, 'yaw': -90, 'roll': 0}}, snap_to_ground=False)['returnValue']
    setp(outside, {'leverKind': 'Ascent', 'placement': 'Outside', 'elevatorActor': elevator})
    setp(ref(outside['refPath'] + '.LeverMesh'), {'bVisible': False, 'bHiddenInGame': True})
    visual_bp = asset('/Game/Game/Environment/Hub/Blueprints/BP_HubVisual_Lever')
    visual = tool(SCENE + 'add_to_scene_from_class', actor_type=ref(visual_bp['refPath'] + '_C'),
                  name='Hub_Visual_LeverOutside', parent=outside,
                  xform={'location': {'x': 0, 'y': 0, 'z': 0}}, snap_to_ground=False)['returnValue']
    setp(visual, {'gameplayActor': outside})
    for actor in [well, outside, visual]:
        tool(SCENE + 'set_actor_folder', actor=actor, folder_path='Hub/Gameplay')
    for path, transform in baseline.items():
        assert tool(ACTOR + 'get_actor_transform', actor=ref(path))['returnValue'] == transform, path
    assert len(find_actors('DigetherHubWellActor')) == 1
    assert len(find_actors('DigetherElevatorLeverActor')) == 3
    assert tool(ASSET + 'save_assets', asset_paths=[well_bp['refPath'], level])['returnValue']
    return {'saved': [well_bp['refPath'], level], 'well': well, 'outsideLever': outside,
            'visual': visual, 'preservedTransforms': baseline}


def complete_result_widget():
    bp = asset('/Game/Game/UI/Hub/WBP_HubRunResult')
    setp(bp, {'bIsFocusable': True})
    tree = widgets(bp)['widgets']
    by_name = {entry['widgetName']: entry for entry in tree}
    assert 'Border_Background' not in by_name
    background = tool(UMG + 'AddWidget', widgetBlueprint=bp, widgetClass=ref('/Script/UMG.Border'),
                      widgetDisplayName='Border_Background', parentWidget=by_name['Canvas_Root']['widget'],
                      childIndex=0)['returnValue']
    setp(background['widget'], {'brushColor': {'r': 0, 'g': 0, 'b': 0, 'a': 1},
                               'visibility': 'HitTestInvisible'})
    setp(background['slot'], {'layoutData': {'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}},
                                           'offsets': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0}}})
    setp(by_name['Text_Close']['widget'], {'text': '닫기', 'autoWrapText': False, 'justification': 'Center'})
    assert tool(UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp)['returnValue']
    assert tool(ASSET + 'save_assets', asset_paths=[bp['refPath']])['returnValue']
    return {'saved': bp, 'focusable': getp(bp, ['bIsFocusable'])}


def preflight():
    assert not tool('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    result = {'level': tool(SCENE + 'get_current_level')['returnValue']}
    result['controllers'] = {}
    for name in ['BP_DigetherHubPlayerController', 'BP_DigetherGameplayPlayerController_Test']:
        bp = asset('/Game/Game/Blueprints/' + name)
        result['controllers'][name] = getp(bp, ['interactAction', 'interactMappingContext',
            'hudWidgetClass', 'runResultWidgetClass', 'gameOverCreditsWidgetClass',
            'pourAction', 'pourMappingContext'])
    result['widgets'] = {}
    for path in ['/Game/Frontend/UI/MainMenu/WBP_CreateRoomPanel',
                 '/Game/Game/UI/Hub/WBP_HubRunResult', '/Game/Game/UI/Hub/WBP_HubRunHud']:
        tree = widgets(asset(path))
        result['widgets'][path] = tree
    result['actors'] = {}
    for cls, names in [('DigetherElevatorLeverActor', ['leverKind', 'placement', 'elevatorActor']),
                       ('DigetherDepositStationActor', ['stationId']),
                       ('DigetherHubWellActor', ['suctionRadiusCm'])]:
        result['actors'][cls] = [{'actor': actor, 'properties': getp(actor, names)}
                                 for actor in find_actors(cls)]
    return result


def run():
    return {'core': connect_core(), 'result': complete_result_widget(),
            'presentation': connect_presentation(), 'hub': connect_hub_map()}
