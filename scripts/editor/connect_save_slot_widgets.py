"""Attach the existing C++ save-slot bindings to the room creation panel.

Run through the live MCP sandbox after backing up WBP_CreateRoomPanel. This
script only edits that existing WBP; slot contents and hosting remain C++ work.
The root editor operator owns execution, compilation and package saving.
"""
import json

UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
PANEL_PATH = '/Game/Frontend/UI/MainMenu/WBP_CreateRoomPanel'


def ref(path):
    return {'refPath': path}


def call(name, **arguments):
    return execute_tool(name, json.dumps(arguments))


def valid_ref(value):
    return isinstance(value, dict) and bool(value.get('refPath'))


def inspect(obj, names):
    available = json.loads(call(OBJ + 'list_properties', instance=obj)['returnValue'])
    assert all(name in available for name in names), (obj, names)
    return json.loads(call(OBJ + 'get_properties', instance=obj,
                           properties=names)['returnValue'])


def setp(obj, values):
    # UMG widgets and slots require discovery and readback before every write.
    inspect(obj, list(values))
    assert call(OBJ + 'set_properties', instance=obj,
                values=json.dumps(values))['returnValue'], (obj, list(values))


def copy_style(source, destination, candidates):
    source_names = json.loads(call(OBJ + 'list_properties', instance=source)['returnValue'])
    target_names = json.loads(call(OBJ + 'list_properties', instance=destination)['returnValue'])
    names = [name for name in candidates if name in source_names and name in target_names]
    if names:
        values = inspect(source, names)
        setp(destination, values)
    return names


def tree(bp):
    return call(UMG + 'GetWidgets', widgetBlueprint=bp)['returnValue']['widgets']


def actual(widgets, name):
    matches = [info for info in widgets if info['widgetName'] == name
               and valid_ref(info['widget'])]
    assert len(matches) <= 1, name
    return matches[0] if matches else None


def ensure(bp, name, widget_class, parent, index):
    widgets = tree(bp)
    info = actual(widgets, name)
    if info is None:
        # GetWidgets also lists unfulfilled optional C++ bindings with widget=None.
        info = call(UMG + 'AddWidget', widgetBlueprint=bp,
                    widgetClass=ref(widget_class), widgetDisplayName=name,
                    parentWidget=parent, childIndex=index)['returnValue']
    else:
        siblings = [entry for entry in widgets if entry['parent'] == parent
                    and valid_ref(entry['widget'])]
        position = next((i for i, entry in enumerate(siblings)
                         if entry['widget'] == info['widget']), -1)
        if info['parent'] != parent or position != index:
            info = call(UMG + 'MoveWidget', widgetBlueprint=bp,
                        widget=info['widget'], newParent=parent,
                        childIndex=index)['returnValue']
    assert valid_ref(info['widget']) and valid_ref(info['slot']), name
    return info


def padding(left=0, top=0, right=0, bottom=0):
    return {'left': left, 'top': top, 'right': right, 'bottom': bottom}


def label(bp, name, text, parent, index, source):
    info = ensure(bp, name, '/Script/CommonUI.CommonTextBlock', parent, index)
    copy_style(source, info['widget'], ['style', 'font', 'colorAndOpacity',
        'shadowColorAndOpacity', 'shadowOffset'])
    setp(info['widget'], {'text': text, 'autoWrapText': True,
                         'visibility': 'HitTestInvisible'})
    return info


def run():
    assert not call('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    assert call(ASSET + 'exists', path=PANEL_PATH)['returnValue'], PANEL_PATH
    bp = ref(PANEL_PATH + '.WBP_CreateRoomPanel')
    before = tree(bp)
    required = ['VerticalBox_0', 'CommonTextBlock_1', 'CommonTextBlock_4',
                'HorizontalBox_1', 'HorizontalBox_2', 'Check_Public']
    originals = {name: actual(before, name) for name in required}
    assert all(originals.values()), 'Unexpected existing room panel layout'
    content = originals['VerticalBox_0']['widget']
    source_text = originals['CommonTextBlock_1']['widget']
    source_hint = originals['CommonTextBlock_4']['widget']

    # Place the slot controls before the existing hint and action-button rows.
    group = ensure(bp, 'VerticalBox_SaveSlotControls', '/Script/UMG.VerticalBox',
                   content, 3)
    copy_style(originals['HorizontalBox_1']['slot'], group['slot'],
               ['padding', 'size', 'horizontalAlignment', 'verticalAlignment'])
    setp(group['slot'], {'padding': padding(top=12, bottom=12),
                        'horizontalAlignment': 'HAlign_Fill'})

    slot_row = ensure(bp, 'HorizontalBox_SaveSlot', '/Script/UMG.HorizontalBox',
                      group['widget'], 0)
    setp(slot_row['slot'], {'padding': padding(bottom=8),
                           'horizontalAlignment': 'HAlign_Fill'})
    slot_label = label(bp, 'Text_SaveSlotLabel', '진행 슬롯', slot_row['widget'],
                       0, source_text)
    setp(slot_label['widget'], {'minDesiredWidth': 220, 'autoWrapText': False})
    setp(slot_label['slot'], {'padding': padding(right=20),
                             'verticalAlignment': 'VAlign_Center'})
    combo = ensure(bp, 'Combo_SaveSlot', '/Script/UMG.ComboBoxString',
                   slot_row['widget'], 1)

    # Reuse the existing frontend combo style; never write its C++-owned options.
    settings = ref('/Game/Frontend/UI/Settings/WBP_SettingsPanel.WBP_SettingsPanel')
    settings_combo = actual(tree(settings), 'Combo_Resolution')
    assert settings_combo, 'Missing existing frontend combo style source'
    copied_combo = copy_style(settings_combo['widget'], combo['widget'],
        ['widgetStyle', 'itemStyle', 'contentPadding', 'font', 'foregroundColor',
         'maxListHeight', 'hasDownArrow', 'bIsFocusable'])
    setp(combo['widget'], {'visibility': 'Visible'})
    setp(combo['slot'], {'size': {'sizeRule': 'Fill', 'value': 1},
                        'verticalAlignment': 'VAlign_Center'})

    summary = label(bp, 'Text_SaveSlotSummary', '슬롯을 선택하면 저장 정보가 표시됩니다.',
                    group['widget'], 1, source_hint)
    setp(summary['widget'], {'wrapTextAt': 560})
    setp(summary['slot'], {'padding': padding(bottom=10),
                          'horizontalAlignment': 'HAlign_Fill'})

    new_row = ensure(bp, 'HorizontalBox_NewGame', '/Script/UMG.HorizontalBox',
                     group['widget'], 2)
    check = ensure(bp, 'Check_NewGame', '/Script/UMG.CheckBox', new_row['widget'], 0)
    copied_check = copy_style(originals['Check_Public']['widget'], check['widget'],
                             ['widgetStyle'])
    # The native panel keeps the selected slot unless the player opts into reset.
    setp(check['widget'], {'checkedState': 'Unchecked', 'visibility': 'Visible'})
    setp(check['slot'], {'padding': padding(right=10),
                        'verticalAlignment': 'VAlign_Center'})
    new_label = label(bp, 'Text_NewGameLabel', '새 게임으로 시작 (선택한 슬롯 초기화)',
                      new_row['widget'], 1, source_hint)
    setp(new_label['slot'], {'size': {'sizeRule': 'Fill', 'value': 1},
                            'verticalAlignment': 'VAlign_Center'})

    for info in [combo, summary, check]:
        call(UMG + 'ToggleWidgetAsVariable', widgetBlueprint=bp,
             widget=info['widget'], bIsVariable=True)
    assert call(UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp)['returnValue']
    after = tree(bp)
    for name in ['Combo_SaveSlot', 'Check_NewGame', 'Text_SaveSlotSummary']:
        assert actual(after, name), name
    # Existing room name, public visibility, hint and action buttons stay intact.
    for name in required + ['Input_RoomName', 'Btn_Create', 'Btn_Close']:
        assert actual(after, name)['widget'] == actual(before, name)['widget'], name
    assert call(ASSET + 'save_assets', asset_paths=[PANEL_PATH])['returnValue']
    return {'saved': PANEL_PATH, 'bindings': {
        name: actual(after, name) for name in
        ['Combo_SaveSlot', 'Check_NewGame', 'Text_SaveSlotSummary']},
        'contentOrder': [info['widgetName'] for info in after
                         if info['parent'] == content and valid_ref(info['widget'])],
        'copiedComboStyle': copied_combo, 'copiedCheckStyle': copied_check,
        'newGameDefault': inspect(check['widget'], ['checkedState'])}
