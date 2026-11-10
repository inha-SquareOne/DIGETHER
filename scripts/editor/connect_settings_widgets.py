"""Connect the existing settings C++ contracts through the live MCP sandbox.

The root editor operator owns execution, backups, compilation and package saves.
The default run() only inspects. Choose an explicit phase through run_phase().
New packages refuse replacement. Existing settings/lobby layout must be supplied
from the operator's live GetWidgets inspection; this file never guesses parents.
"""
import json

UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
BP = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'

FOLDER = '/Game/Frontend/UI/Settings'
SETTINGS = FOLDER + '/WBP_SettingsPanel'
KEY_ROW = FOLDER + '/WBP_KeyBindingRow'
VOICE_ROW = FOLDER + '/WBP_PlayerVoiceRow'
VOICE_LIST = FOLDER + '/WBP_PlayerVoiceList'
LOBBY = '/Game/Frontend/UI/Lobby/WBP_LobbyScreen'
BUTTON_CLASS = '/Game/Frontend/UI/Common/WBP_Button.WBP_Button_C'

NEW_TYPES = {
    KEY_ROW: 'DigetherKeyBindingRowWidget',
    VOICE_ROW: 'DigetherPlayerVoiceRowWidget',
    VOICE_LIST: 'DigetherPlayerVoiceListWidget',
}
NEW_BINDINGS = {
    KEY_ROW: {'Text_Action': 'CommonTextBlock', 'Btn_Key': 'WBP_Button_C'},
    VOICE_ROW: {'Text_PlayerName': 'CommonTextBlock', 'Slider_Volume': 'Slider',
                'Text_VolumeValue': 'CommonTextBlock', 'Check_Mute': 'CheckBox'},
    VOICE_LIST: {'Panel_Players': 'VerticalBox'},
}
LOBBY_LIST_NAME = 'VoiceList_Players'
SETTINGS_BINDINGS = {
    'Slider_Brightness': 'Slider', 'Text_BrightnessValue': 'CommonTextBlock',
    'Slider_FieldOfView': 'Slider', 'Text_FieldOfViewValue': 'CommonTextBlock',
    'Slider_ResolutionScale': 'Slider', 'Text_ResolutionScaleValue': 'CommonTextBlock',
    'Check_MotionBlur': 'CheckBox', 'Slider_Voice': 'Slider',
    'Text_VoiceValue': 'CommonTextBlock', 'Slider_MicGain': 'Slider',
    'Text_MicGainValue': 'CommonTextBlock', 'Slider_MicThreshold': 'Slider',
    'Text_MicThresholdValue': 'CommonTextBlock', 'ProgressBar_MicLevel': 'ProgressBar',
    'Text_MicStatus': 'CommonTextBlock', 'Page_Keys': 'VerticalBox',
    'Panel_KeyRows': 'VerticalBox',
}
MIC_MONITOR_BINDINGS = {'Btn_MicMonitor': 'WBP_Button_C'}
ORIGINAL_BINDINGS = [
    'Tabs_Category', 'Switcher_Pages', 'Page_Graphics', 'Page_Sound', 'Page_Controls',
    'Combo_Resolution', 'Combo_WindowMode', 'Combo_Quality', 'Combo_FrameLimit',
    'Check_VSync', 'Slider_Master', 'Slider_Effects', 'Slider_Music',
    'Text_MasterValue', 'Text_EffectsValue', 'Text_MusicValue', 'Slider_Sensitivity',
    'Text_SensitivityValue', 'Check_InvertY', 'Text_Notice', 'Btn_Apply',
    'Btn_Defaults', 'Btn_Close',
]


def ref(path):
    return {'refPath': path}


def valid_ref(value):
    return isinstance(value, dict) and bool(value.get('refPath'))


def actual_widget(entry):
    value = entry.get('widget')
    return valid_ref(value) and ':WidgetTree.' in value['refPath']


def call(name, **arguments):
    result = execute_tool(name, json.dumps(arguments))
    assert isinstance(result, dict), (name, result)
    # execute_tool raises on failure; its strict result mapping exposes schema keys only.
    return result


def available(obj):
    return json.loads(call(OBJ + 'list_properties', instance=obj)['returnValue'])


def getp(obj, names):
    names = list(names)
    found = available(obj)
    assert all(name in found for name in names), (obj, names, found)
    return json.loads(call(OBJ + 'get_properties', instance=obj,
                           properties=names)['returnValue'])


def setp(obj, values):
    # 매 쓰기 전 정확한 프로퍼티 이름·현재 값을 조회
    getp(obj, values.keys())
    assert call(OBJ + 'set_properties', instance=obj,
                values=json.dumps(values))['returnValue'], (obj, values)
    return getp(obj, values.keys())


def asset(path):
    assert call(ASSET + 'exists', path=path)['returnValue'], path
    result = call(ASSET + 'load_asset', asset_path=path)['returnValue']
    assert valid_ref(result), (path, result)
    return result


def generated(path):
    return ref(path + '.' + path.rsplit('/', 1)[-1] + '_C')


def info(bp):
    return call(UMG + 'GetWidgets', widgetBlueprint=bp)['returnValue']


def actual(entries, name):
    matches = [entry for entry in entries if entry['widgetName'] == name
               and actual_widget(entry)]
    assert len(matches) <= 1, (name, matches)
    return matches[0] if matches else None


def require(entries, name, class_suffix=None):
    entry = actual(entries, name)
    assert entry, ('Missing actual widget', name)
    if class_suffix:
        # GetWidgets uses Blueprint asset paths for generated widget classes.
        class_path = entry['widgetClassPath']['refPath']
        actual_class = class_path[:-2] if class_path.endswith('_C') else class_path
        expected = class_suffix[:-2] if class_suffix.endswith('_C') else class_suffix
        assert actual_class.endswith(expected), entry
    return entry


def add(bp, cls, name, parent=None, index=-1, variable=False):
    assert not actual(info(bp)['widgets'], name), ('Widget already exists', name)
    entry = call(UMG + 'AddWidget', widgetBlueprint=bp, widgetClass=ref(cls),
                 widgetDisplayName=name, parentWidget=parent,
                 childIndex=index)['returnValue']
    assert valid_ref(entry['widget']), (name, entry)
    if parent is not None:
        assert valid_ref(entry['slot']), (name, entry)
    if variable:
        call(UMG + 'ToggleWidgetAsVariable', widgetBlueprint=bp,
             widget=entry['widget'], bIsVariable=True)
    return entry


def umg(bp, cls, name, parent=None, index=-1, variable=False):
    return add(bp, '/Script/UMG.' + cls, name, parent, index, variable)


def margin(left=0, top=0, right=0, bottom=0):
    return {'left': left, 'top': top, 'right': right, 'bottom': bottom}


def copy_style(source, destination, names):
    source_names = available(source)
    target_names = available(destination)
    selected = [name for name in names if name in source_names and name in target_names]
    assert selected, ('No shared style properties', source, destination, names)
    return setp(destination, getp(source, selected))


def styles():
    widgets = info(asset(SETTINGS))['widgets']
    return {
        'text': require(widgets, 'Text_MasterValue', 'CommonTextBlock')['widget'],
        'slider': require(widgets, 'Slider_Master', 'Slider')['widget'],
        'check': require(widgets, 'Check_VSync', 'CheckBox')['widget'],
    }


def text(bp, name, label, parent, source, index=-1, variable=False):
    entry = add(bp, '/Script/CommonUI.CommonTextBlock', name, parent, index, variable)
    copy_style(source, entry['widget'], ['style', 'font', 'colorAndOpacity',
               'shadowColorAndOpacity', 'shadowOffset'])
    setp(entry['widget'], {'text': label, 'autoWrapText': False,
                          'visibility': 'HitTestInvisible'})
    return entry


def slider(bp, name, parent, source, minimum=0, maximum=1, value=1):
    entry = umg(bp, 'Slider', name, parent, variable=True)
    copy_style(source, entry['widget'], ['widgetStyle', 'sliderBarColor',
               'sliderHandleColor', 'orientation', 'stepSize', 'mouseUsesStep'])
    setp(entry['widget'], {'minValue': minimum, 'maxValue': maximum,
                          'value': value, 'visibility': 'Visible'})
    return entry


def checkbox(bp, name, parent, source, checked=False):
    entry = umg(bp, 'CheckBox', name, parent, variable=True)
    copy_style(source, entry['widget'], ['widgetStyle'])
    setp(entry['widget'], {'checkedState': 'Checked' if checked else 'Unchecked',
                          'visibility': 'Visible'})
    return entry


def row_slot(entry, fill=False, left=0, top=0, right=0, bottom=0):
    values = {'padding': margin(left, top, right, bottom),
              'verticalAlignment': 'VAlign_Center'}
    if fill:
        values['size'] = {'sizeRule': 'Fill', 'value': 1}
    return setp(entry['slot'], values)


def size_box(bp, name, parent=None, width=None, height=None, index=-1):
    entry = umg(bp, 'SizeBox', name, parent, index)
    values = {}
    if width is not None:
        values.update({'bOverride_WidthOverride': True, 'widthOverride': width})
    if height is not None:
        values.update({'bOverride_HeightOverride': True, 'heightOverride': height})
    if values:
        setp(entry['widget'], values)
    return entry


def new_bp(path, native_class):
    assert not call(ASSET + 'exists', path=path)['returnValue'], ('Refusing replacement', path)
    result = call(UMG + 'CreateWidgetBlueprint', folderPath=FOLDER,
                  assetName=path.rsplit('/', 1)[-1],
                  parentClass=ref('/Script/DIGETHER.' + native_class))['returnValue']
    assert valid_ref(result), result
    # 새 패키지의 빈 기본 Canvas만 제거 가능, 기존 사용자 패키지는 여기로 진입 불가
    entries = [entry for entry in info(result)['widgets'] if actual_widget(entry)]
    if entries:
        assert len(entries) == 1 and entries[0]['widgetClassPath']['refPath'].endswith('CanvasPanel'), entries
        assert not valid_ref(entries[0].get('parent')), entries
        assert call(UMG + 'RemoveWidget', widgetBlueprint=result,
                    widget=entries[0]['widget'])['returnValue']
    return result


def assert_bindings(path, expected, parent_class=None):
    bp = asset(path)
    snapshot = info(bp)
    if parent_class:
        assert snapshot['info']['parentClass']['refPath'] == '/Script/DIGETHER.' + parent_class, snapshot['info']
    found = {}
    for name, suffix in expected.items():
        entry = require(snapshot['widgets'], name, suffix)
        assert entry['bIsVariable'], ('Native binding not variable', name)
        found[name] = entry
    return found


def finish(path, bindings, parent_class=None, preserved=None, relocations=None,
           sibling_orders=None):
    bp = asset(path)
    assert call(UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp)['returnValue'], path
    # Python BlueprintTools는 경고·오류 발생 시 예외를 던짐
    call(BP + 'compile_blueprint', blueprint=bp, warnings_as_errors=True)
    found = assert_bindings(path, bindings, parent_class)
    if preserved is not None:
        preserve_tree(preserved, info(bp)['widgets'], relocations, sibling_orders)
    assert call(ASSET + 'save_assets', asset_paths=[path])['returnValue'], path
    assert not call(ASSET + 'is_dirty', asset_path=path)['returnValue'], path
    return {'path': path, 'bindings': found, 'dirty': False}


def preserve_tree(before, after, relocations=None, sibling_orders=None):
    """Existing object identities, hierarchy and relative sibling order survive."""
    relocations = relocations or {}
    originals = [entry for entry in before if actual_widget(entry)]
    for old in originals:
        current = require(after, old['widgetName'])
        moved = relocations.get(old['widgetName'])
        for field in ['widget', 'widgetClassPath', 'bIsVariable']:
            assert current[field] == old[field], ('Existing widget changed', old['widgetName'], field)
        if moved:
            assert current['parent'] == moved['parent'], current
            assert valid_ref(current['slot']), current
            assert getp(current['slot'], moved['slot_values'].keys()) == moved['slot_values'], current
        else:
            assert current['parent'] == old['parent'], current
            assert current['slot'] == old['slot'], current
    expected_parents = {entry['widgetName']: (
        relocations[entry['widgetName']]['parent'] if entry['widgetName'] in relocations
        else entry.get('parent')) for entry in originals}
    parents = list(expected_parents.values())
    for parent in parents:
        approved = [order for order in (sibling_orders or []) if order['parent'] == parent]
        if approved:
            assert len(approved) == 1, approved
            current_order = [entry['widgetName'] for entry in after if actual_widget(entry)
                             and entry.get('parent') == parent]
            assert current_order == approved[0]['names'], (parent, current_order, approved)
            continue
        old_order = [entry['widgetName'] for entry in originals
                     if expected_parents[entry['widgetName']] == parent]
        old_names = set(old_order)
        new_order = [entry['widgetName'] for entry in after if actual_widget(entry)
                     and entry.get('parent') == parent and entry['widgetName'] in old_names]
        assert old_order == new_order, ('Existing sibling order changed', parent)


def scroll_page_contents(bp, page_name, before):
    """Keep the bound page directly under the switcher and scroll its contents."""
    page = require(before, page_name, 'VerticalBox')
    assert page['parent'] == require(before, 'Switcher_Pages')['widget'], page
    suffix = page_name.replace('Page_', '') + 'Page'
    scroll_name = 'Scroll_' + suffix
    content_name = 'Column_' + suffix
    assert actual(before, scroll_name) is None and actual(before, content_name) is None, suffix
    children = [entry for entry in before if actual_widget(entry)
                and entry.get('parent') == page['widget']]
    assert children, ('Unexpected empty existing page', page_name)
    # Capture the old VerticalBox slot values before MoveWidget destroys its slot.
    slots = {entry['widgetName']: getp(entry['slot'], [
        'padding', 'size', 'horizontalAlignment', 'verticalAlignment']) for entry in children}
    scroll = umg(bp, 'ScrollBox', scroll_name, page['widget'])
    setp(scroll['slot'], {'size': {'sizeRule': 'Fill', 'value': 1},
                         'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
    content = umg(bp, 'VerticalBox', content_name, scroll['widget'])
    setp(content['slot'], {'horizontalAlignment': 'HAlign_Fill', 'padding': margin(right=10)})
    moved = {}
    for index, old in enumerate(children):
        current = call(UMG + 'MoveWidget', widgetBlueprint=bp, widget=old['widget'],
                       newParent=content['widget'], childIndex=index)['returnValue']
        assert current['widget'] == old['widget'] and valid_ref(current['slot']), current
        setp(current['slot'], slots[old['widgetName']])
        moved[old['widgetName']] = {'parent': content['widget'],
                                  'slot_values': slots[old['widgetName']]}
    return content['widget'], moved


def numeric_readback(widget, expected):
    values = getp(widget, expected.keys())
    for name, value in expected.items():
        assert abs(values[name] - value) < 0.0001, (name, value, values)
    return values


def settings_row(bp, name, parent):
    outer = size_box(bp, 'Size_' + name, parent, height=42)
    setp(outer['slot'], {'padding': margin(top=5, bottom=5),
                        'horizontalAlignment': 'HAlign_Fill'})
    row = umg(bp, 'HorizontalBox', 'Row_' + name, outer['widget'])
    setp(row['slot'], {'horizontalAlignment': 'HAlign_Fill',
                      'verticalAlignment': 'VAlign_Center'})
    return row


def settings_label(bp, name, label, row, source):
    box = size_box(bp, 'Size_Label_' + name, row, width=220)
    row_slot(box, right=20)
    caption = text(bp, 'Text_Label_' + name, label, box['widget'], source)
    setp(caption['slot'], {'horizontalAlignment': 'HAlign_Fill',
                          'verticalAlignment': 'VAlign_Center'})
    return caption


def setting_slider(bp, suffix, caption, parent, source, low, high, value, label):
    row = settings_row(bp, suffix, parent)
    settings_label(bp, suffix, caption, row['widget'], source['text'])
    control = slider(bp, 'Slider_' + suffix, row['widget'], source['slider'], low, high, value)
    row_slot(control, fill=True, right=20)
    display = text(bp, 'Text_' + suffix + 'Value', label, row['widget'], source['text'], variable=True)
    setp(display['widget'], {'minDesiredWidth': 90})
    row_slot(display)
    return numeric_readback(control['widget'], {'minValue': low, 'maxValue': high, 'value': value})


def setting_checkbox(bp, suffix, caption, parent, source, checked):
    row = settings_row(bp, suffix, parent)
    settings_label(bp, suffix, caption, row['widget'], source['text'])
    control = checkbox(bp, 'Check_' + suffix, row['widget'], source['check'], checked)
    row_slot(control)
    assert getp(control['widget'], ['checkedState'])['checkedState'] == (
        'Checked' if checked else 'Unchecked'), suffix
    return control


def create_key_row():
    source = styles()
    bp = new_bp(KEY_ROW, NEW_TYPES[KEY_ROW])
    outer = size_box(bp, 'Size_KeyRow', height=54)
    row = umg(bp, 'HorizontalBox', 'Row_Key', outer['widget'])
    setp(row['slot'], {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Center'})
    label = text(bp, 'Text_Action', '동작', row['widget'], source['text'], variable=True)
    row_slot(label, fill=True, right=20)
    key_box = size_box(bp, 'Size_KeyButton', row['widget'], width=240, height=42)
    row_slot(key_box)
    button = add(bp, BUTTON_CLASS, 'Btn_Key', key_box['widget'], variable=True)
    setp(button['widget'], {'buttonText': '키 지정'})
    setp(button['slot'], {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
    return finish(KEY_ROW, NEW_BINDINGS[KEY_ROW], NEW_TYPES[KEY_ROW])


def create_voice_row():
    source = styles()
    bp = new_bp(VOICE_ROW, NEW_TYPES[VOICE_ROW])
    outer = size_box(bp, 'Size_VoiceRow', height=54)
    row = umg(bp, 'HorizontalBox', 'Row_Voice', outer['widget'])
    setp(row['slot'], {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Center'})
    player = text(bp, 'Text_PlayerName', '동료', row['widget'], source['text'], variable=True)
    setp(player['widget'], {'minDesiredWidth': 160})
    row_slot(player, right=16)
    volume = slider(bp, 'Slider_Volume', row['widget'], source['slider'], maximum=2)
    row_slot(volume, fill=True, right=12)
    value = text(bp, 'Text_VolumeValue', '100%', row['widget'], source['text'], variable=True)
    setp(value['widget'], {'minDesiredWidth': 76})
    row_slot(value, right=12)
    mute = checkbox(bp, 'Check_Mute', row['widget'], source['check'])
    row_slot(mute, right=8)
    caption = text(bp, 'Text_MuteLabel', '음소거', row['widget'], source['text'])
    row_slot(caption)
    return finish(VOICE_ROW, NEW_BINDINGS[VOICE_ROW], NEW_TYPES[VOICE_ROW])


def create_voice_list():
    source = styles()
    assert_bindings(VOICE_ROW, NEW_BINDINGS[VOICE_ROW], NEW_TYPES[VOICE_ROW])
    bp = new_bp(VOICE_LIST, NEW_TYPES[VOICE_LIST])
    root = umg(bp, 'VerticalBox', 'Column_VoiceList')
    title = text(bp, 'Text_VoiceListTitle', '동료 목소리 · 이 PC에서만 적용', root['widget'], source['text'])
    setp(title['slot'], {'padding': margin(bottom=10), 'horizontalAlignment': 'HAlign_Fill'})
    scroll = umg(bp, 'ScrollBox', 'Scroll_Players', root['widget'])
    setp(scroll['slot'], {'size': {'sizeRule': 'Fill', 'value': 1}, 'horizontalAlignment': 'HAlign_Fill'})
    players = umg(bp, 'VerticalBox', 'Panel_Players', scroll['widget'], variable=True)
    setp(players['slot'], {'horizontalAlignment': 'HAlign_Fill', 'padding': margin(right=6)})
    assert call(UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp)['returnValue'], VOICE_LIST
    setp(bp, {'rowClass': generated(VOICE_ROW)})
    result = finish(VOICE_LIST, NEW_BINDINGS[VOICE_LIST], NEW_TYPES[VOICE_LIST])
    result['defaults'] = getp(bp, ['rowClass'])
    return result


def create_new_widgets():
    assert_not_pie()
    for path in NEW_TYPES:
        assert not call(ASSET + 'exists', path=path)['returnValue'], ('Refusing replacement', path)
    return {'keyRow': create_key_row(), 'voiceRow': create_voice_row(), 'voiceList': create_voice_list()}


def assert_not_pie():
    assert not call('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue'], 'PIE must be stopped'


def inspect_state():
    result = {}
    for path in [SETTINGS, LOBBY] + list(NEW_TYPES):
        exists = call(ASSET + 'exists', path=path)['returnValue']
        result[path] = {'exists': exists}
        if exists:
            bp = asset(path)
            result[path].update({'tree': info(bp), 'dirty': call(ASSET + 'is_dirty', asset_path=path)['returnValue']})
    return result


def extend_settings_panel(plan):
    """Append one/all sections using the operator's inspected parent names."""
    assert_not_pie()
    assert isinstance(plan, dict), 'Pass the live-inspected settings layout plan'
    section = plan.get('section', 'all')
    assert section in ['all', 'graphics', 'sound', 'keys'], section
    bp = asset(SETTINGS)
    before = info(bp)['widgets']
    for name in ORIGINAL_BINDINGS:
        require(before, name)
    defaults_before = getp(bp, ['tabButtonClass'])
    source = styles()
    expected = {}
    values = {}
    relocations = {}

    groups = {
        'graphics': ['Slider_Brightness', 'Text_BrightnessValue', 'Slider_FieldOfView',
                     'Text_FieldOfViewValue', 'Slider_ResolutionScale',
                     'Text_ResolutionScaleValue', 'Check_MotionBlur'],
        'sound': ['Slider_Voice', 'Text_VoiceValue', 'Slider_MicGain',
                  'Text_MicGainValue', 'Slider_MicThreshold',
                  'Text_MicThresholdValue', 'ProgressBar_MicLevel', 'Text_MicStatus'],
        'keys': ['Page_Keys', 'Panel_KeyRows'],
    }
    selected = list(groups) if section == 'all' else [section]
    # A failed phase is restored from the operator backup, never silently rebuilt.
    for group in selected:
        for name in groups[group]:
            assert actual(before, name) is None, ('Refusing extension replacement', name)
            expected[name] = SETTINGS_BINDINGS[name]

    if 'graphics' in selected:
        assert plan['graphics_parent'] == 'Page_Graphics', plan
        parent, moved = scroll_page_contents(bp, plan['graphics_parent'], before)
        relocations.update(moved)
        values['brightness'] = setting_slider(bp, 'Brightness', '밝기', parent, source,
                                               1.6, 3.0, 2.2, '2.20')
        values['fieldOfView'] = setting_slider(bp, 'FieldOfView', '시야각', parent, source,
                                                60, 110, 90, '90°')
        values['resolutionScale'] = setting_slider(bp, 'ResolutionScale', '해상도 비율',
                                                    parent, source, 0.5, 1.0, 1.0, '100%')
        setting_checkbox(bp, 'MotionBlur', '모션 블러', parent, source, True)

    if 'sound' in selected:
        assert plan['sound_parent'] == 'Page_Sound', plan
        parent, moved = scroll_page_contents(bp, plan['sound_parent'], before)
        relocations.update(moved)
        values['voice'] = setting_slider(bp, 'Voice', '음성 음량', parent, source,
                                          0.0, 1.0, 1.0, '100%')
        values['micGain'] = setting_slider(bp, 'MicGain', '마이크 증폭', parent, source,
                                            0.5, 3.0, 1.0, '1.00x')
        values['micThreshold'] = setting_slider(bp, 'MicThreshold', '마이크 입력 기준',
                                                 parent, source, 0.0, 0.3, 0.08, '0.08')
        row = settings_row(bp, 'MicLevel', parent)
        settings_label(bp, 'MicLevel', '마이크 입력', row['widget'], source['text'])
        meter_box = size_box(bp, 'Size_MicMeter', row['widget'], height=16)
        row_slot(meter_box, fill=True, right=20)
        meter = umg(bp, 'ProgressBar', 'ProgressBar_MicLevel', meter_box['widget'], variable=True)
        setp(meter['widget'], {'percent': 0.0, 'visibility': 'Visible'})
        setp(meter['slot'], {'horizontalAlignment': 'HAlign_Fill',
                            'verticalAlignment': 'VAlign_Fill'})
        status = text(bp, 'Text_MicStatus', '마이크 입력 확인', row['widget'], source['text'], variable=True)
        setp(status['widget'], {'minDesiredWidth': 150})
        row_slot(status)

    if 'keys' in selected:
        assert_bindings(KEY_ROW, NEW_BINDINGS[KEY_ROW], NEW_TYPES[KEY_ROW])
        switcher = require(before, plan['keys_parent'], 'CommonAnimatedSwitcher')['widget']
        assert plan['keys_parent'] == 'Switcher_Pages', plan
        siblings = [entry for entry in before if actual_widget(entry) and entry.get('parent') == switcher]
        assert [entry['widgetName'] for entry in siblings] == [
            'Page_Graphics', 'Page_Sound', 'Page_Controls'], siblings
        assert plan['keys_index'] == len(siblings), plan
        keys = umg(bp, 'VerticalBox', 'Page_Keys', switcher, plan['keys_index'], variable=True)
        setp(keys['slot'], {'horizontalAlignment': 'HAlign_Fill',
                           'verticalAlignment': 'VAlign_Fill'})
        hint = text(bp, 'Text_KeyHint', '키를 선택한 뒤 새 키를 누르세요. Esc로 취소합니다.',
                    keys['widget'], source['text'])
        setp(hint['slot'], {'padding': margin(top=8, bottom=12),
                           'horizontalAlignment': 'HAlign_Fill'})
        scroll = umg(bp, 'ScrollBox', 'Scroll_KeyRows', keys['widget'])
        setp(scroll['slot'], {'size': {'sizeRule': 'Fill', 'value': 1},
                             'horizontalAlignment': 'HAlign_Fill'})
        rows = umg(bp, 'VerticalBox', 'Panel_KeyRows', scroll['widget'], variable=True)
        setp(rows['slot'], {'padding': margin(right=8), 'horizontalAlignment': 'HAlign_Fill'})
        assert call(UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp)['returnValue'], SETTINGS
        setp(bp, {'keyRowClass': generated(KEY_ROW)})

    result = finish(SETTINGS, expected, 'DigetherSettingsPanelWidget', before, relocations)
    after = info(bp)['widgets']
    assert getp(bp, ['tabButtonClass']) == defaults_before, 'Existing tab class changed'
    if 'keys' in selected:
        assert require(after, 'Page_Keys')['parent'] == require(after, 'Switcher_Pages')['widget']
        result['keyRowClass'] = getp(bp, ['keyRowClass'])
        assert result['keyRowClass']['keyRowClass'] == generated(KEY_ROW), result
    result['section'] = section
    result['sliderReadback'] = values
    result['preservedWidgets'] = len([entry for entry in before if actual_widget(entry)])
    return result


def connect_mic_monitor(plan):
    """Add the native monitor button after the operator-inspected meter row."""
    assert_not_pie()
    assert isinstance(plan, dict), 'Pass the live-inspected microphone layout plan'
    assert call(ASSET + 'can_edit_asset', asset_path=SETTINGS)['returnValue'], SETTINGS
    assert not call(ASSET + 'is_dirty', asset_path=SETTINGS)['returnValue'], 'Save existing edits first'
    bp = asset(SETTINGS)
    before = info(bp)['widgets']
    parent = require(before, plan['parent_name'], 'VerticalBox')['widget']
    meter_row = require(before, plan['after_name'], 'SizeBox')
    assert meter_row['parent'] == parent, meter_row
    siblings = [entry for entry in before if actual_widget(entry) and entry.get('parent') == parent]
    index = [entry['widgetName'] for entry in siblings].index(plan['after_name']) + 1
    assert plan['index'] == index, plan
    for name in ['Size_MicMonitor', 'Row_MicMonitor', 'Size_MicMonitorButton', 'Btn_MicMonitor']:
        assert actual(before, name) is None, ('Refusing monitor replacement', name)
    defaults = getp(bp, ['tabButtonClass', 'keyRowClass'])
    row = settings_row(bp, 'MicMonitor', parent)
    moved = call(UMG + 'MoveWidget', widgetBlueprint=bp, widget=require(info(bp)['widgets'],
                 'Size_MicMonitor')['widget'], newParent=parent, childIndex=index)['returnValue']
    setp(moved['slot'], {'padding': margin(top=5, bottom=5), 'horizontalAlignment': 'HAlign_Fill'})
    box = size_box(bp, 'Size_MicMonitorButton', row['widget'], width=240, height=42)
    row_slot(box, left=240)
    button = add(bp, BUTTON_CLASS, 'Btn_MicMonitor', box['widget'], variable=True)
    setp(button['widget'], {'buttonText': '내 목소리 듣기'})
    setp(button['slot'], {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
    bindings = dict(SETTINGS_BINDINGS)
    bindings.update(MIC_MONITOR_BINDINGS)
    result = finish(SETTINGS, bindings, 'DigetherSettingsPanelWidget', before)
    assert getp(bp, ['tabButtonClass', 'keyRowClass']) == defaults, defaults
    after = info(bp)['widgets']
    order = [entry['widgetName'] for entry in after if actual_widget(entry) and entry.get('parent') == parent]
    assert order[index] == 'Size_MicMonitor', order
    result['siblingOrder'] = order
    result['preservedWidgets'] = len([entry for entry in before if actual_widget(entry)])
    return result


def place_lobby_list(plan):
    """Use an inspected parent/index/slot, optionally with a fixed-height wrapper."""
    assert_not_pie()
    assert isinstance(plan, dict), 'Pass the live-inspected lobby placement plan'
    assert isinstance(plan.get('slot_values'), dict) and plan['slot_values'], plan
    assert 'index' in plan and 'parent_name' in plan and 'parent_class' in plan, plan
    assert_bindings(VOICE_LIST, NEW_BINDINGS[VOICE_LIST], NEW_TYPES[VOICE_LIST])
    bp = asset(LOBBY)
    before = info(bp)['widgets']
    assert actual(before, LOBBY_LIST_NAME) is None, 'Refusing replacement of lobby voice list'
    parent = require(before, plan['parent_name'], plan['parent_class'])['widget']
    siblings = [entry for entry in before if actual_widget(entry) and entry.get('parent') == parent]
    index = plan['index']
    assert isinstance(index, int) and 0 <= index <= len(siblings), plan
    # The operator supplies placement only after reading this exact live slot type.
    if 'container_height' in plan:
        assert plan['parent_class'] == 'VerticalBox', plan
        assert plan['container_height'] > 0, plan
        assert actual(before, 'Size_LobbyVoiceList') is None, 'Refusing replacement of lobby wrapper'
        box = size_box(bp, 'Size_LobbyVoiceList', parent,
                       height=plan['container_height'], index=index)
        slot_readback = setp(box['slot'], plan['slot_values'])
        widget = add(bp, generated(VOICE_LIST)['refPath'], LOBBY_LIST_NAME,
                     box['widget'], variable=True)
        setp(widget['slot'], {'horizontalAlignment': 'HAlign_Fill',
                             'verticalAlignment': 'VAlign_Fill'})
        inserted_name = 'Size_LobbyVoiceList'
    else:
        widget = add(bp, generated(VOICE_LIST)['refPath'], LOBBY_LIST_NAME,
                     parent, index, variable=True)
        slot_readback = setp(widget['slot'], plan['slot_values'])
        inserted_name = LOBBY_LIST_NAME
    result = finish(LOBBY, {LOBBY_LIST_NAME: 'WBP_PlayerVoiceList_C'},
                    'DigetherLobbyScreenWidget', before)
    after = info(bp)['widgets']
    inserted = require(after, inserted_name)
    assert inserted['parent'] == parent, inserted
    ordered = [entry['widgetName'] for entry in after if actual_widget(entry)
               and entry.get('parent') == parent]
    assert ordered[index] == inserted_name, ordered
    result['slotReadback'] = slot_readback
    result['siblingOrder'] = ordered
    return result


def polish_settings_panel():
    """Apply only the visibility, placement and palette fixes observed in PIE QA."""
    assert_not_pie()
    bp = asset(SETTINGS)
    before = info(bp)['widgets']
    controls = require(before, 'Page_Controls', 'VerticalBox')['widget']
    sound = require(before, 'Column_SoundPage', 'VerticalBox')['widget']
    first = require(before, 'HorizontalBox_8', 'HorizontalBox')
    second = require(before, 'HorizontalBox_9', 'HorizontalBox')
    mic_row = require(before, 'Row_MicrophoneMode', 'HorizontalBox')
    shake_row = require(before, 'Row_ReduceShake', 'HorizontalBox')
    mic_label = require(before, 'Text_MicrophoneMode', 'TextBlock')
    mic_combo = require(before, 'Combo_MicrophoneMode', 'ComboBoxString')
    shake_label = require(before, 'Text_ReduceShake', 'TextBlock')
    shake_check = require(before, 'Check_ReduceShake', 'CheckBox')
    hint_controls = require(before, 'CommonTextBlock_16', 'CommonTextBlock')
    hint_sound = require(before, 'CommonTextBlock_12', 'CommonTextBlock')
    meter = require(before, 'ProgressBar_MicLevel', 'ProgressBar')
    assert first['parent'] == controls and second['parent'] == controls, controls
    assert mic_row['parent'] in [sound, controls], mic_row
    assert shake_row['parent'] == controls and hint_controls['parent'] == controls
    defaults = getp(bp, ['tabButtonClass', 'keyRowClass'])
    source_label = require(before, 'CommonTextBlock_1', 'CommonTextBlock')['widget']
    source_combo = require(before, 'Combo_Resolution', 'ComboBoxString')['widget']
    source_check = require(before, 'Check_VSync', 'CheckBox')['widget']
    source_slider = require(before, 'Slider_Master', 'Slider')['widget']
    relocations = {}
    slot_fields = ['padding', 'size', 'horizontalAlignment', 'verticalAlignment']

    def move_existing(entry, parent, index, slot_values):
        moved = call(UMG + 'MoveWidget', widgetBlueprint=bp, widget=entry['widget'],
                     newParent=parent, childIndex=index)['returnValue']
        assert moved['widget'] == entry['widget'] and valid_ref(moved['slot']), moved
        setp(moved['slot'], slot_values)
        relocations[entry['widgetName']] = {'parent': parent, 'slot_values': slot_values}
        return moved

    # RegisterTabs keeps the controls page itself directly in Switcher_Pages.
    for row, index in [(mic_row, 2), (shake_row, 3)]:
        old_slot = getp(row['slot'], slot_fields)
        old_slot.update({'padding': margin(top=5, bottom=5),
                         'horizontalAlignment': 'HAlign_Fill',
                         'verticalAlignment': 'VAlign_Center'})
        move_existing(row, controls, index, old_slot)
        assert setp(row['widget'], {'visibility': 'Visible'})['visibility'] == 'Visible'

    def fixed_cell(entry, row, name, index, width):
        current = info(bp)['widgets']
        box = actual(current, name)
        if box is None:
            box = size_box(bp, name, row['widget'], width=width, height=42, index=index)
        else:
            require(current, name, 'SizeBox')
            assert box['parent'] == row['widget'], box
            numeric_readback(box['widget'], {'widthOverride': width, 'heightOverride': 42})
        row_slot(box, right=20 if index == 0 else 0)
        latest = require(info(bp)['widgets'], entry['widgetName'])
        child_slot = {'padding': margin(), 'horizontalAlignment': 'HAlign_Fill',
                      'verticalAlignment': 'VAlign_Center'}
        if latest['parent'] != box['widget']:
            assert latest['parent'] == row['widget'], latest
            move_existing(latest, box['widget'], 0, child_slot)
        else:
            setp(latest['slot'], child_slot)
        return box

    fixed_cell(mic_label, mic_row, 'Size_MicrophoneModeLabel', 0, 220)
    fixed_cell(mic_combo, mic_row, 'Size_MicrophoneModeControl', 1, 360)
    fixed_cell(shake_label, shake_row, 'Size_ReduceShakeLabel', 0, 220)
    setp(shake_check['slot'], {'padding': margin(), 'verticalAlignment': 'VAlign_Center'})
    for label in [mic_label, shake_label]:
        copy_style(source_label, label['widget'], ['font', 'colorAndOpacity',
                   'shadowColorAndOpacity', 'shadowOffset'])
    copy_style(source_combo, mic_combo['widget'], ['widgetStyle', 'itemStyle',
               'contentPadding', 'font', 'foregroundColor', 'maxListHeight',
               'hasDownArrow', 'bIsFocusable'])
    copy_style(source_check, shake_check['widget'], ['widgetStyle'])

    messages = {
        'CommonTextBlock_16': '키 설정 탭에서 키를 바꿀 수 있습니다.',
        'CommonTextBlock_12': '음량은 적용을 눌러 저장합니다.',
    }
    for entry in [hint_controls, hint_sound]:
        message = messages[entry['widgetName']]
        assert setp(entry['widget'], {'text': message})['text'] == message

    palette = getp(source_slider, ['sliderBarColor', 'sliderHandleColor'])
    background = {key: (value * 0.2 if key != 'a' else value)
                  for key, value in palette['sliderBarColor'].items()}
    gold = palette['sliderHandleColor']
    meter_style = getp(meter['widget'], ['widgetStyle'])['widgetStyle']
    assert 'backgroundImage' in meter_style and 'fillImage' in meter_style, meter_style
    meter_style['backgroundImage']['tintColor'] = {
        'specifiedColor': background, 'colorUseRule': 'UseColor_Specified'}
    # Keep one gold tint: the existing fill brush is white, fillColor carries gold.
    meter_style['fillImage']['tintColor'] = {
        'specifiedColor': {'r': 1, 'g': 1, 'b': 1, 'a': 1},
        'colorUseRule': 'UseColor_Specified'}
    meter_values = setp(meter['widget'], {'widgetStyle': meter_style,
                                        'fillColorAndOpacity': gold})
    actual_background = meter_values['widgetStyle']['backgroundImage']['tintColor']['specifiedColor']
    for channel in ['r', 'g', 'b', 'a']:
        assert abs(actual_background[channel] - background[channel]) < 0.0001
        assert abs(meter_values['fillColorAndOpacity'][channel] - gold[channel]) < 0.0001

    expected_order = [first['widgetName'], second['widgetName'], mic_row['widgetName'],
                      shake_row['widgetName'], hint_controls['widgetName']]
    expected_bindings = dict(SETTINGS_BINDINGS)
    expected_bindings.update({'Combo_MicrophoneMode': 'ComboBoxString',
                              'Check_ReduceShake': 'CheckBox'})
    result = finish(SETTINGS, expected_bindings, 'DigetherSettingsPanelWidget',
                    before, relocations, [{'parent': controls, 'names': expected_order}])
    assert getp(bp, ['tabButtonClass', 'keyRowClass']) == defaults, defaults
    after = info(bp)['widgets']
    for name in ORIGINAL_BINDINGS:
        require(after, name)
    for name in ['Row_MicrophoneMode', 'Row_ReduceShake']:
        assert getp(require(after, name)['widget'], ['visibility'])['visibility'] == 'Visible'
    result['controlsOrder'] = expected_order
    result['messages'] = messages
    result['micMeterColors'] = {'background': actual_background,
                                'fill': meter_values['fillColorAndOpacity']}
    result['preservedWidgets'] = len([entry for entry in before if actual_widget(entry)])
    return result


def fit_lobby_voice_list():
    """Shrink only the QA-observed 250px voice wrapper to restore the lobby hint."""
    assert_not_pie()
    bp = asset(LOBBY)
    before = info(bp)['widgets']
    canvas = require(before, 'CanvasPanel_0', 'CanvasPanel')
    column = require(before, 'VerticalBox_0', 'VerticalBox')
    wrapper = require(before, 'Size_LobbyVoiceList', 'SizeBox')
    voice = require(before, LOBBY_LIST_NAME, 'WBP_PlayerVoiceList_C')
    hint = require(before, 'Text_Hint', 'CommonTextBlock')
    assert wrapper['parent'] == column['widget'] and hint['parent'] == column['widget']
    assert voice['parent'] == wrapper['widget'], voice
    previous = getp(wrapper['widget'], ['bOverride_HeightOverride', 'heightOverride'])
    assert previous['bOverride_HeightOverride'], previous
    assert abs(previous['heightOverride'] - 250) < 0.0001, ('Unexpected lobby wrapper height', previous)
    canvas_values = getp(canvas['widget'], list(available(canvas['widget'])))
    canvas_slots = {entry['widgetName']: getp(entry['slot'], list(available(entry['slot'])))
                    for entry in before if actual_widget(entry)
                    and entry.get('parent') == canvas['widget']}
    assert canvas_slots, 'Unexpected empty lobby Canvas'
    setp(wrapper['widget'], {'heightOverride': 150})
    numeric_readback(wrapper['widget'], {'heightOverride': 150})
    result = finish(LOBBY, {LOBBY_LIST_NAME: 'WBP_PlayerVoiceList_C'},
                    'DigetherLobbyScreenWidget', before)
    after = info(bp)['widgets']
    current = require(after, 'Size_LobbyVoiceList', 'SizeBox')
    current_values = getp(current['widget'], ['bOverride_HeightOverride', 'heightOverride'])
    assert current_values['bOverride_HeightOverride'], current_values
    assert abs(current_values['heightOverride'] - 150) < 0.0001, current_values
    assert getp(canvas['widget'], canvas_values.keys()) == canvas_values, 'Lobby Canvas properties changed'
    for name, values in canvas_slots.items():
        assert getp(require(after, name)['slot'], values.keys()) == values, ('Lobby Canvas slot changed', name)
    result['heightBefore'] = previous['heightOverride']
    result['heightAfter'] = current_values['heightOverride']
    result['preservedCanvasChildren'] = list(canvas_slots)
    result['preservedWidgets'] = len([entry for entry in before if actual_widget(entry)])
    return result


def validate_all():
    assert_not_pie()
    result = {}
    for path, native_class in NEW_TYPES.items():
        result[path] = finish(path, NEW_BINDINGS[path], native_class)
    settings_bindings = dict(SETTINGS_BINDINGS)
    settings_bindings.update(MIC_MONITOR_BINDINGS)
    result[SETTINGS] = finish(SETTINGS, settings_bindings, 'DigetherSettingsPanelWidget')
    result[LOBBY] = finish(LOBBY, {LOBBY_LIST_NAME: 'WBP_PlayerVoiceList_C'},
                           'DigetherLobbyScreenWidget')
    settings = asset(SETTINGS)
    entries = info(settings)['widgets']
    for name in ORIGINAL_BINDINGS:
        require(entries, name)
    keys = require(entries, 'Page_Keys')
    assert keys['parent'] == require(entries, 'Switcher_Pages')['widget'], keys
    key_panel = require(entries, 'Panel_KeyRows')
    assert key_panel['parent'] == require(entries, 'Scroll_KeyRows')['widget'], key_panel
    assert getp(settings, ['keyRowClass'])['keyRowClass'] == generated(KEY_ROW)
    assert getp(asset(VOICE_LIST), ['rowClass'])['rowClass'] == generated(VOICE_ROW)
    result['volumeBounds'] = numeric_readback(
        require(info(asset(VOICE_ROW))['widgets'], 'Slider_Volume')['widget'],
        {'minValue': 0.0, 'maxValue': 2.0, 'value': 1.0})
    return result


def run_phase(phase, plan=None):
    assert_not_pie()
    phases = {'inspect': inspect_state, 'create': create_new_widgets,
              'new-key-row': create_key_row, 'new-voice-row': create_voice_row,
              'new-voice-list': create_voice_list, 'polish': polish_settings_panel,
              'lobby-fit': fit_lobby_voice_list, 'validate': validate_all}
    if phase == 'extend':
        return extend_settings_panel(plan)
    if phase == 'lobby':
        return place_lobby_list(plan)
    if phase == 'mic-monitor':
        return connect_mic_monitor(plan)
    assert phase in phases, ('Unknown phase', phase)
    return phases[phase]()


def run():
    return run_phase('inspect')
