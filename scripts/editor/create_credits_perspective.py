"""Prepare a true perspective retainer for the existing credits WBP via MCP.

Root operator: back up the WBP and inspect MaterialTools/UMG schemas first.
Set CP_OUTPUT_PROPERTIES to the actual schema enum strings for emissive and
opacity before run(). The runtime C++ tilt hook is a separate guarded operation
because its localized node IDs must be discovered in the live graph first.
This script creates a new material and moves existing widgets without replacing
their objects, text bindings, C++ scrolling, background or input handlers.
"""
import json
import math

CP_MAT = 'editor_toolset.toolsets.material.MaterialTools.'
CP_OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
CP_UMG = 'UMGToolSet.UMGToolSet.'
CP_ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
CP_BP = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
CP_WIDGET_PATH = '/Game/Game/UI/Hub/WBP_HubGameOverCredits'
CP_MATERIAL_PATH = '/Game/Game/UI/Hub/Materials/M_CreditsPerspective'
CP_OUTPUT_PROPERTIES = None
CP_BLOCK_NAMES = ['Text_Period', 'Text_TotalEarned', 'Text_EarnedBlock',
                  'Text_HiddenBlock', 'Text_DeathLossBlock']
CP_HLSL = """// UI Retainer Box inverse homography; Texture is supplied by the retainer.
// Provisional default 25 degrees; constrain only the UI projection near its pole.
float TiltK = tan(radians(clamp(TiltDegrees, 0.0, 75.0)));
float OnePlusK = 1.0 + TiltK;
float Denominator = 1.0 + TiltK * UV.y;
if (abs(OnePlusK) < 0.0001 || abs(Denominator) < 0.0001)
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
float V = UV.y * OnePlusK / Denominator;
float WidthScale = Denominator / OnePlusK;
float U = (UV.x - 0.5) / WidthScale + 0.5;
float2 SourceUV = float2(U, V);
if (any(SourceUV < 0.0) || any(SourceUV > 1.0))
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
return Texture2DSample(InputTexture, InputTextureSampler, SourceUV);
"""


def cp_ref(path):
    return {'refPath': path}


def cp_call(name, **arguments):
    return execute_tool(name, json.dumps(arguments))


def cp_properties(obj):
    return json.loads(cp_call(CP_OBJ + 'list_properties', instance=obj)['returnValue'])


def cp_get(obj, names):
    available = cp_properties(obj)
    assert all(name in available for name in names), (obj, names)
    return json.loads(cp_call(CP_OBJ + 'get_properties', instance=obj,
                              properties=names)['returnValue'])


def cp_set(obj, values):
    cp_get(obj, list(values))
    assert cp_call(CP_OBJ + 'set_properties', instance=obj,
                   values=json.dumps(values))['returnValue'], (obj, list(values))


def cp_widgets(bp):
    return cp_call(CP_UMG + 'GetWidgets', widgetBlueprint=bp)['returnValue']['widgets']


def cp_find(entries, name):
    matches = [entry for entry in entries if entry['widgetName'] == name]
    assert len(matches) == 1, (name, matches)
    return matches[0]


def cp_slot_snapshot(slot):
    names = cp_properties(slot)
    kept = [name for name in ['layoutData', 'zOrder', 'bAutoSize', 'autoSize'] if name in names]
    assert 'layoutData' in kept and 'zOrder' in kept, (slot, names)
    assert len([name for name in kept if name in ['bAutoSize', 'autoSize']]) == 1, names
    return cp_get(slot, kept)


def cp_add_expression(material, name, x, y):
    classes = cp_call(CP_MAT + 'list_expression_classes', material_or_function=material,
                      search=name)['returnValue']
    assert name in json.dumps(classes), (name, classes)
    return cp_call(CP_MAT + 'add_expression', material_or_function=material,
                   expression_class=cp_ref('/Script/Engine.' + name), x=x, y=y)['returnValue']


def cp_default_output(expression):
    outputs = cp_call(CP_MAT + 'get_expression_output_names', expression=expression)['returnValue']
    assert len(outputs) == 1, (expression, outputs)
    return outputs[0]


def cp_connect(source, destination, input_name):
    inputs = cp_call(CP_MAT + 'get_expression_input_names', expression=destination)['returnValue']
    assert input_name in inputs, (destination, input_name, inputs)
    cp_call(CP_MAT + 'connect_expressions', from_expression=source,
            from_output_name=cp_default_output(source), to_expression=destination,
            to_input_name=input_name)


def cp_create_material(output_properties):
    assert set(output_properties) == {'color', 'opacity'}, output_properties
    assert not cp_call(CP_ASSET + 'exists', path=CP_MATERIAL_PATH)['returnValue'], \
        'Existing material must not be overwritten'
    material = cp_call(CP_MAT + 'create_material', folder_path='/Game/Game/UI/Hub/Materials',
                       asset_name='M_CreditsPerspective')['returnValue']
    # Retainer의 premultiplied RGBA를 다시 alpha 곱하지 않고 합성
    cp_set(material, {'materialDomain': 'MD_UI', 'blendMode': 'BLEND_AlphaComposite'})
    texture = cp_add_expression(material, 'MaterialExpressionTextureObjectParameter', -800, -200)
    white = '/Engine/EngineResources/WhiteSquareTexture'
    assert cp_call(CP_ASSET + 'exists', path=white)['returnValue'], white
    cp_set(texture, {'parameterName': 'Texture', 'texture': cp_ref(white + '.WhiteSquareTexture')})
    uv = cp_add_expression(material, 'MaterialExpressionTextureCoordinate', -800, 0)
    cp_set(uv, {'coordinateIndex': 0})
    tilt = cp_add_expression(material, 'MaterialExpressionScalarParameter', -800, 200)
    cp_set(tilt, {'parameterName': 'TiltDegrees', 'defaultValue': 25.0})
    custom = cp_add_expression(material, 'MaterialExpressionCustom', -420, 0)
    cp_set(custom, {'code': CP_HLSL, 'description': 'Credits inverse homography',
                    'outputType': 'CMOT_Float4'})
    # MCP 배열 diff는 기존 항목 수정과 크기 증가를 한 번에 허용하지 않음
    cp_set(custom, {'inputs': []})
    cp_set(custom, {'inputs': [{'inputName': name} for name in ['InputTexture', 'UV', 'TiltDegrees']]})
    for source, input_name in [(texture, 'InputTexture'), (uv, 'UV'), (tilt, 'TiltDegrees')]:
        cp_connect(source, custom, input_name)
    rgb = cp_add_expression(material, 'MaterialExpressionComponentMask', -50, -80)
    alpha = cp_add_expression(material, 'MaterialExpressionComponentMask', -50, 100)
    cp_set(rgb, {'r': True, 'g': True, 'b': True, 'a': False})
    cp_set(alpha, {'r': False, 'g': False, 'b': False, 'a': True})
    for mask, output_name in [(rgb, output_properties['color']), (alpha, output_properties['opacity'])]:
        inputs = cp_call(CP_MAT + 'get_expression_input_names', expression=mask)['returnValue']
        assert len(inputs) == 1, inputs
        cp_connect(custom, mask, inputs[0])
        cp_call(CP_MAT + 'connect_to_output', expression=mask,
                output_name=cp_default_output(mask), material_property=output_name)
        source = cp_call(CP_MAT + 'get_property_input', material=material,
                         material_property=output_name)['returnValue']
        assert source['expression'] == mask, source
    wiring = cp_call(CP_MAT + 'get_expression_inputs', material_or_function=material,
                     expression=custom)['returnValue']
    assert {entry['input_name']: entry['expression'] for entry in wiring} == \
        {'InputTexture': texture, 'UV': uv, 'TiltDegrees': tilt}, wiring
    # Raises on actual shader errors; no success is inferred from asset creation.
    cp_call(CP_MAT + 'recompile', material_or_function=material)
    return material


def create_credits_perspective(output_properties):
    assert not cp_call('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    assert cp_call(CP_ASSET + 'exists', path=CP_WIDGET_PATH)['returnValue'], CP_WIDGET_PATH
    assert not cp_call(CP_ASSET + 'exists', path=CP_MATERIAL_PATH)['returnValue'], CP_MATERIAL_PATH
    bp = cp_ref(CP_WIDGET_PATH + '.WBP_HubGameOverCredits')
    before = cp_widgets(bp)
    names = [entry['widgetName'] for entry in before]
    assert 'Retainer_CreditsPerspective' not in names and 'Canvas_Perspective' not in names
    root = cp_find(before, 'Canvas_Root')
    scroll = cp_find(before, 'Panel_Scroll')
    assert root['widgetClassPath']['refPath'] == '/Script/UMG.CanvasPanel'
    assert scroll['widgetClassPath']['refPath'] == '/Script/UMG.CanvasPanel'
    assert scroll['parent'] == root['widget'], scroll
    for name in CP_BLOCK_NAMES + ['Border_Background', 'Button_FastForward', 'Text_FastForward']:
        cp_find(before, name)
    scroll_slot = cp_slot_snapshot(scroll['slot'])
    scroll_transform = cp_get(scroll['widget'], ['renderTransform', 'renderTransformPivot'])
    roots = [entry for entry in before if entry['parent'] == root['widget']]
    scroll_index = next(index for index, entry in enumerate(roots) if entry['widget'] == scroll['widget'])
    untouched_slots = {entry['widgetName']: cp_slot_snapshot(entry['slot'])
                       for entry in roots if entry['widget'] != scroll['widget']}

    material = cp_create_material(output_properties)
    retainer = cp_call(CP_UMG + 'AddWidget', widgetBlueprint=bp,
                       widgetClass=cp_ref('/Script/UMG.RetainerBox'),
                       widgetDisplayName='Retainer_CreditsPerspective',
                       parentWidget=root['widget'], childIndex=scroll_index)['returnValue']
    cp_call(CP_UMG + 'ToggleWidgetAsVariable', widgetBlueprint=bp,
            widget=retainer['widget'], bIsVariable=True)
    retainer_names = cp_properties(retainer['widget'])
    retaining = [name for name in ['bRetainRender', 'retainRender'] if name in retainer_names]
    assert len(retaining) == 1, retainer_names
    cp_set(retainer['widget'], {retaining[0]: True, 'effectMaterial': material,
           'textureParameter': 'Texture', 'renderOnPhase': True, 'renderOnInvalidation': True,
           'phase': 0, 'phaseCount': 1, 'bShowEffectsInDesigner': True,
           'visibility': 'HitTestInvisible'})
    retainer_slot = cp_slot_snapshot(retainer['slot'])
    full = {'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}},
            'offsets': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0},
            'alignment': {'x': 0, 'y': 0}}
    retainer_slot['layoutData'] = full
    retainer_slot['zOrder'] = scroll_slot['zOrder']
    for name in ['bAutoSize', 'autoSize']:
        if name in retainer_slot:
            retainer_slot[name] = False
    cp_set(retainer['slot'], retainer_slot)
    perspective = cp_call(CP_UMG + 'AddWidget', widgetBlueprint=bp,
                          widgetClass=cp_ref('/Script/UMG.CanvasPanel'),
                          widgetDisplayName='Canvas_Perspective',
                          parentWidget=retainer['widget'], childIndex=0)['returnValue']
    moved = cp_call(CP_UMG + 'MoveWidget', widgetBlueprint=bp, widget=scroll['widget'],
                    newParent=perspective['widget'], childIndex=0)['returnValue']
    assert moved['widget'] == scroll['widget']
    cp_set(moved['slot'], scroll_slot)
    assert cp_slot_snapshot(moved['slot']) == scroll_slot
    assert cp_get(scroll['widget'], ['renderTransform', 'renderTransformPivot']) == scroll_transform
    assert cp_call(CP_UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp)['returnValue']
    after = cp_widgets(bp)
    for entry in before:
        current = cp_find(after, entry['widgetName'])
        assert current['widget'] == entry['widget'], entry['widgetName']
        if entry['widgetName'] != 'Panel_Scroll':
            assert current['parent'] == entry['parent'], entry['widgetName']
            assert current['slot'] == entry['slot'], entry['widgetName']
    for name, saved in untouched_slots.items():
        assert cp_slot_snapshot(cp_find(after, name)['slot']) == saved, name
    assert cp_find(after, 'Panel_Scroll')['parent'] == perspective['widget']
    assert cp_find(after, 'Border_Background')['parent'] == root['widget']
    assert cp_find(after, 'Button_FastForward')['parent'] == root['widget']
    assert cp_call(CP_ASSET + 'save_assets', asset_paths=[CP_MATERIAL_PATH, CP_WIDGET_PATH])['returnValue']
    return {'saved': [CP_MATERIAL_PATH, CP_WIDGET_PATH], 'material': material,
            'retainer': retainer['widget'], 'preservedScrollWidget': scroll['widget'],
            'preservedScrollSlotValues': scroll_slot, 'defaultTiltDegrees': 25.0,
            'defaultTiltK': math.tan(math.radians(25.0)),
            'runtimeTiltConnected': False,
            'next': 'Discover live node IDs, then call cp_connect_runtime_tilt(node_types)'}


def cp_connect_runtime_tilt(node_types):
    """Insert before the existing shown chain, using five live-discovered IDs.

    Keys: retainer_get, effect_get, tilt_get, scalar_set, valid. Confirm effect_get
    is URetainerBox.GetEffectMaterial, scalar_set is the MID scalar setter, and
    valid is the engine IsValid macro. GetEffectMaterial is pure in this engine.
    A null MID preserves the old input chain.
    """
    required = {'retainer_get', 'effect_get', 'tilt_get', 'scalar_set', 'valid'}
    assert set(node_types) == required, node_types
    assert not cp_call('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    bp = cp_ref(CP_WIDGET_PATH + '.WBP_HubGameOverCredits')
    retainer = cp_find(cp_widgets(bp), 'Retainer_CreditsPerspective')
    assert cp_get(retainer['widget'], ['effectMaterial', 'textureParameter']) == {
        'effectMaterial': cp_ref(CP_MATERIAL_PATH + '.M_CreditsPerspective'), 'textureParameter': 'Texture'}
    graph = cp_call(CP_BP + 'get_graph', blueprint=bp, graph_name='EventGraph')['returnValue']
    shown_ref = cp_call(CP_BP + 'add_event', blueprint=bp, event_name='BP_OnCreditsShown',
                        position={'x': 0, 'y': 100})['returnValue']
    shown = cp_call(CP_BP + 'get_node_infos', nodes=[shown_ref])['returnValue'][0]

    def pin(info, name, is_input):
        pins = [entry for entry in info['input_pins' if is_input else 'output_pins'] if entry['name'] == name]
        assert len(pins) == 1, (info['type_id'], name, pins)
        return pins[0]

    previous = pin(shown, 'then', False)['connected_pins']
    assert len(previous) == 1, 'Preserve the one existing shown input chain'
    previous_node = cp_call(CP_BP + 'get_node_infos', nodes=[previous[0]['node']])['returnValue'][0]
    assert previous_node['type_id'] != node_types['valid'], 'The runtime tilt hook is already inserted'
    made = {}
    for index, (key, type_id) in enumerate(node_types.items()):
        found = cp_call(CP_BP + 'find_node_types', graph=graph,
                        type_id_filter=type_id, context_pins=[])['returnValue']
        assert type_id in found, (key, type_id, found)
        cp_call(CP_BP + 'get_node_type_pins', graph=graph, type_id=type_id)
        arguments = {'graph': graph, 'type_id': type_id,
                     'pos': {'x': 300 + index * 280, 'y': -600}}
        if key == 'scalar_set':
            # 같은 ID의 MPC 전역 setter와 MID 인스턴스 setter 구분
            arguments['declaring_class'] = cp_ref('/Script/Engine.MaterialInstanceDynamic')
        node = cp_call(CP_BP + 'create_node', **arguments)['returnValue']
        made[key] = cp_call(CP_BP + 'get_node_infos', nodes=[node])['returnValue'][0]

    def wire(source, source_pin, destination, destination_pin):
        cp_call(CP_BP + 'connect_pins', output_pin=pin(source, source_pin, False)['pin_id'],
                input_pin=pin(destination, destination_pin, True)['pin_id'])

    getter_outputs = made['retainer_get']['output_pins']
    assert len(getter_outputs) == 1, getter_outputs
    wire(made['retainer_get'], getter_outputs[0]['name'], made['effect_get'], 'self')
    wire(made['effect_get'], 'ReturnValue', made['scalar_set'], 'self')
    wire(made['tilt_get'], 'ReturnValue', made['scalar_set'], 'Value')
    wire(made['effect_get'], 'ReturnValue', made['valid'], 'InputObject')
    cp_call(CP_BP + 'set_pin_value', pin=pin(made['scalar_set'], 'ParameterName', True)['pin_id'], value='TiltDegrees')
    # Validate all new pins before moving the pre-existing execution wire.
    assert not any(p['name'] == 'execute' for p in made['effect_get']['input_pins'])
    pin(made['scalar_set'], 'execute', True)
    pin(made['scalar_set'], 'then', False)
    pin(made['valid'], 'exec', True)
    pin(made['valid'], 'Is Valid', False)
    pin(made['valid'], 'Is Not Valid', False)
    cp_call(CP_BP + 'break_pins', output_pin=pin(shown, 'then', False)['pin_id'], input_pin=previous[0])
    wire(shown, 'then', made['valid'], 'exec')
    wire(made['valid'], 'Is Valid', made['scalar_set'], 'execute')
    cp_call(CP_BP + 'connect_pins', output_pin=pin(made['scalar_set'], 'then', False)['pin_id'], input_pin=previous[0])
    cp_call(CP_BP + 'connect_pins', output_pin=pin(made['valid'], 'Is Not Valid', False)['pin_id'], input_pin=previous[0])
    cp_call(CP_BP + 'compile_blueprint', blueprint=bp, warnings_as_errors=True)
    assert cp_call(CP_ASSET + 'save_assets', asset_paths=[CP_WIDGET_PATH])['returnValue']
    return {'saved': [CP_WIDGET_PATH], 'preservedContinuation': previous[0],
            'runtimeTiltConnected': True, 'nodeTypes': node_types}


def run():
    assert CP_OUTPUT_PROPERTIES is not None, 'Set MaterialProperty enum strings from live describe_toolset first'
    return create_credits_perspective(CP_OUTPUT_PROPERTIES)
