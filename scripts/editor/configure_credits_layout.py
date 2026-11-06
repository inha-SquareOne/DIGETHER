"""Flow existing credit blocks inside their C++-controlled scroll panel.

Execute only through the root operator's live MCP sandbox after a package
backup. The 80-unit gaps are a provisional designer layout, not server tuning.
Final perspective, fonts and readability still require design approval and QA.
"""
import json

UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
ASSET_PATH = '/Game/Game/UI/Hub/WBP_HubGameOverCredits'
BLOCKS = ['Text_Period', 'Text_TotalEarned', 'Text_EarnedBlock',
          'Text_HiddenBlock', 'Text_DeathLossBlock']


def ref(path):
    return {'refPath': path}


def call(name, **arguments):
    return execute_tool(name, json.dumps(arguments))


def valid_ref(value):
    return isinstance(value, dict) and bool(value.get('refPath'))


def property_names(obj):
    return json.loads(call(OBJ + 'list_properties', instance=obj)['returnValue'])


def getp(obj, names):
    available = property_names(obj)
    assert all(name in available for name in names), (obj, names)
    return json.loads(call(OBJ + 'get_properties', instance=obj,
                           properties=names)['returnValue'])


def setp(obj, values):
    getp(obj, list(values))
    assert call(OBJ + 'set_properties', instance=obj,
                values=json.dumps(values))['returnValue'], (obj, list(values))


def widgets(bp):
    return call(UMG + 'GetWidgets', widgetBlueprint=bp)['returnValue']['widgets']


def find(entries, name):
    matches = [entry for entry in entries if entry['widgetName'] == name
               and valid_ref(entry['widget'])]
    assert len(matches) <= 1, name
    return matches[0] if matches else None


def run():
    assert not call('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    assert call(ASSET + 'exists', path=ASSET_PATH)['returnValue'], ASSET_PATH
    bp = ref(ASSET_PATH + '.WBP_HubGameOverCredits')
    before = widgets(bp)
    preserved = {name: find(before, name) for name in ['Panel_Scroll'] + BLOCKS}
    assert all(preserved.values()), 'Missing existing C++ credit bindings'
    scroll = preserved['Panel_Scroll']['widget']
    assert preserved['Panel_Scroll']['widgetClassPath']['refPath'] == '/Script/UMG.CanvasPanel'
    fonts = {name: getp(preserved[name]['widget'], ['font'])['font'] for name in BLOCKS}

    content_name = 'VerticalBox_CreditContent'
    content = find(before, content_name)
    if content is None:
        content = call(UMG + 'AddWidget', widgetBlueprint=bp,
                       widgetClass=ref('/Script/UMG.VerticalBox'),
                       widgetDisplayName=content_name, parentWidget=scroll,
                       childIndex=0)['returnValue']
    else:
        assert content['widgetClassPath']['refPath'] == '/Script/UMG.VerticalBox'
        if content['parent'] != scroll:
            content = call(UMG + 'MoveWidget', widgetBlueprint=bp,
                           widget=content['widget'], newParent=scroll,
                           childIndex=0)['returnValue']
    assert valid_ref(content['slot'])
    # The list determines the exact reflected bool name on this engine build.
    slot_names = property_names(content['slot'])
    auto_size = [name for name in ['bAutoSize', 'autoSize'] if name in slot_names]
    assert len(auto_size) == 1, 'Canvas auto-size property was not discovered'
    setp(content['slot'], {auto_size[0]: True, 'layoutData': {
        'anchors': {'minimum': {'x': .5, 'y': 0}, 'maximum': {'x': .5, 'y': 0}},
        'alignment': {'x': .5, 'y': 0},
        'offsets': {'left': 0, 'top': 20, 'right': 0, 'bottom': 0}}})

    for index, name in enumerate(BLOCKS):
        current = widgets(bp)
        info = find(current, name)
        children = [entry for entry in current if entry['parent'] == content['widget']
                    and valid_ref(entry['widget'])]
        position = next((i for i, entry in enumerate(children)
                         if entry['widget'] == info['widget']), -1)
        if info['parent'] != content['widget'] or position != index:
            info = call(UMG + 'MoveWidget', widgetBlueprint=bp,
                        widget=info['widget'], newParent=content['widget'],
                        childIndex=index)['returnValue']
        assert valid_ref(info['slot']), name
        # Font and content remain native bindings; only layout properties change.
        setp(info['widget'], {'autoWrapText': True, 'wrapTextAt': 940,
                             'minDesiredWidth': 940, 'justification': 'Center'})
        setp(info['slot'], {'horizontalAlignment': 'HAlign_Fill',
                           'verticalAlignment': 'VAlign_Top',
                           'padding': {'left': 0, 'top': 0, 'right': 0,
                                       'bottom': 80 if index < len(BLOCKS) - 1 else 0}})

    assert call(UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp)['returnValue']
    after = widgets(bp)
    for name, info in preserved.items():
        assert find(after, name)['widget'] == info['widget'], name
    for name in BLOCKS:
        assert getp(find(after, name)['widget'], ['font'])['font'] == fonts[name], name
    order = [entry['widgetName'] for entry in after
             if entry['parent'] == content['widget'] and valid_ref(entry['widget'])]
    assert order == BLOCKS, order
    # Every preexisting widget remains present, including fast-forward controls.
    for entry in before:
        if valid_ref(entry['widget']):
            assert find(after, entry['widgetName'])['widget'] == entry['widget']
    assert call(ASSET + 'save_assets', asset_paths=[ASSET_PATH])['returnValue']
    return {'saved': ASSET_PATH, 'content': content['widget'], 'order': order,
            'canvasSlot': getp(content['slot'], [auto_size[0], 'layoutData']),
            'fonts': fonts, 'blockGap': 80, 'wrapWidth': 940,
            'preservedBindings': {name: find(after, name)['widget']
                                  for name in ['Panel_Scroll'] + BLOCKS},
            'designStatus': '원근·최종 글꼴·튜닝 가독성 미정, 현재 간격은 디자이너 임시값'}
