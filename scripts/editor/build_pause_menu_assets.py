"""일시정지 입력·위젯·Controller 기본값을 UE MCP sandbox에서 연결한다.

실행자는 에디터 상태·패키지 백업·독점 편집을 확인한 뒤 run_phase()를
호출한다. import/run()은 읽기 전용이며 기존 패키지를 교체하지 않는다.
Settings 폴더·기존 입력·C++·ini는 저장하거나 수정하지 않는다.
"""

import json

ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
BP = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
UMG = 'UMGToolSet.UMGToolSet.'

ACTION = '/Game/Game/Input/Actions/IA_Pause'
CONTEXT = '/Game/Game/Input/IMC_Pause'
MENU = '/Game/Game/UI/Pause/WBP_PauseMenu'
MAIN_MENU = '/Game/Frontend/UI/MainMenu/WBP_MainMenuScreen'
SETTINGS = '/Game/Frontend/UI/Settings/WBP_SettingsPanel'
BUTTON = '/Game/Frontend/UI/Common/WBP_Button.WBP_Button_C'
PARENT = '/Script/DIGETHER.DigetherPauseMenuWidget'
CONTROLLERS = [
    '/Game/Game/Blueprints/BP_DigetherHubPlayerController',
    '/Game/Game/Blueprints/BP_DigetherGameplayPlayerController_Test',
]
BUTTONS = {
    'Btn_Resume': '계속하기',
    'Btn_Settings': '설정',
    'Btn_MainMenu': '메인 메뉴로',
    'Btn_Quit': '게임 종료',
}
BINDINGS = dict.fromkeys(BUTTONS, 'WBP_Button')
BINDINGS.update({'Stack_Panels': 'CommonActivatableWidgetStack',
                 'Panel_Menu': 'VerticalBox'})


def ref(path):
    return {'refPath': path}


def object_path(path):
    return path if '.' in path else path + '.' + path.rsplit('/', 1)[-1]


def generated(path):
    return ref(object_path(path) + '_C')


def valid_ref(value):
    return isinstance(value, dict) and bool(value.get('refPath'))


def clone(value):
    return json.loads(json.dumps(value))


def tool(name, **arguments):
    # execute_tool가 실패 시 예외를 발생시킨다. 반환값은 strict mapping이다.
    return execute_tool(name, json.dumps(arguments))


def available(obj):
    return json.loads(tool(OBJ + 'list_properties', instance=obj)['returnValue'])


def getp(obj, names):
    names = list(names)
    found = available(obj)
    assert all(name in found for name in names), (obj, names, found)
    return json.loads(tool(OBJ + 'get_properties', instance=obj,
                           properties=names)['returnValue'])


def setp(obj, values):
    # 쓰기 전에 실제 프로퍼티와 현재 값을 조회하고 쓰기 뒤 재조회한다.
    getp(obj, list(values))
    assert tool(OBJ + 'set_properties', instance=obj,
                values=json.dumps(values))['returnValue'], (obj, values)
    return getp(obj, list(values))


def exists(path):
    return tool(ASSET + 'exists', path=path)['returnValue']


def asset(path):
    assert exists(path), path
    loaded = tool(ASSET + 'load_asset', asset_path=object_path(path))['returnValue']
    assert valid_ref(loaded), (path, loaded)
    return loaded


def cdo(blueprint):
    result = tool(BP + 'get_default_object', blueprint=blueprint)['returnValue']
    assert valid_ref(result), result
    return result


def assert_not_pie():
    assert not tool('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue'], 'Stop PIE first'


def editable(paths):
    assert_not_pie()
    for path in paths:
        assert tool(ASSET + 'can_edit_asset', asset_path=path)['returnValue'], path


def save(paths):
    paths = list(dict.fromkeys(paths))
    assert paths, 'Empty save list would save unrelated dirty assets'
    assert tool(ASSET + 'save_assets', asset_paths=paths)['returnValue'], paths
    for path in paths:
        assert not tool(ASSET + 'is_dirty', asset_path=path)['returnValue'], path
    return paths


def info(blueprint):
    return tool(UMG + 'GetWidgets', widgetBlueprint=blueprint)['returnValue']


def actual_widget(entry):
    return valid_ref(entry.get('widget')) and ':WidgetTree.' in entry['widget']['refPath']


def actual(entries, name):
    found = [entry for entry in entries if actual_widget(entry)
             and entry['widgetName'] == name]
    assert len(found) <= 1, (name, found)
    return found[0] if found else None


def require(entries, name, suffix=None):
    entry = actual(entries, name)
    assert entry, ('Missing widget', name)
    if suffix:
        class_path = entry['widgetClassPath']['refPath']
        if class_path.endswith('_C'):
            class_path = class_path[:-2]
        assert class_path.endswith(suffix), entry
    return entry


def add(blueprint, cls, name, parent=None, variable=False):
    assert actual(info(blueprint)['widgets'], name) is None, name
    result = tool(UMG + 'AddWidget', widgetBlueprint=blueprint,
                  widgetClass=ref(cls), widgetDisplayName=name,
                  parentWidget=parent, childIndex=-1)['returnValue']
    assert valid_ref(result['widget']), result
    if parent is not None:
        assert valid_ref(result['slot']), result
    if variable:
        tool(UMG + 'ToggleWidgetAsVariable', widgetBlueprint=blueprint,
             widget=result['widget'], bIsVariable=True)
    return result


def umg(blueprint, cls, name, parent=None, variable=False):
    return add(blueprint, '/Script/UMG.' + cls, name, parent, variable)


def margin(left=0, top=0, right=0, bottom=0):
    return {'left': left, 'top': top, 'right': right, 'bottom': bottom}


def overlay_slot(entry, center=False, padding=None):
    values = {'horizontalAlignment': 'HAlign_Center' if center else 'HAlign_Fill',
              'verticalAlignment': 'VAlign_Center' if center else 'VAlign_Fill'}
    if padding is not None:
        values['padding'] = padding
    return setp(entry['slot'], values)


def main_menu_styles():
    entries = [entry for entry in info(asset(MAIN_MENU))['widgets']
               if actual_widget(entry)]
    titles = [entry for entry in entries if 'Title' in entry['widgetName']
              and entry['widgetClassPath']['refPath'].endswith('CommonTextBlock')]
    if not titles:
        # 이 MainMenu에는 제목이 없으며 실제 조회한 Text_Status를 기준으로 쓴다.
        titles = [require(entries, 'Text_Status', 'CommonTextBlock')]
    titles.sort(key=lambda entry: (entry['widgetName'] != 'Text_Title', entry['widgetName']))
    buttons = [entry for entry in entries
               if entry['widgetClassPath']['refPath'].endswith(('WBP_Button', 'WBP_Button_C'))]
    return {'title': titles[0]['widget'],
            'button': buttons[0]['widget'] if buttons else None}


def copy_style(source, destination, names):
    source_names, target_names = available(source), available(destination)
    selected = [name for name in names if name in source_names and name in target_names]
    assert selected, ('No style properties', source, destination, names)
    return setp(destination, getp(source, selected))


def create_input():
    assert_not_pie()
    sources = [('/Game/Game/Input/Actions/IA_HubInteract', ACTION),
               ('/Game/Game/Input/IMC_HubInteract', CONTEXT)]
    for source, destination in sources:
        asset(source)
        assert not exists(destination), ('Refusing replacement', destination)
    for source, destination in sources:
        assert tool(ASSET + 'duplicate', path=source, new_path=destination)['returnValue'], destination
    action, context = asset(ACTION), asset(CONTEXT)
    setp(action, {'valueType': 'Boolean', 'triggers': [], 'modifiers': [],
                  'playerMappableKeySettings': 'None'})
    return configure_input_mappings(action, context)


def configure_input_mappings(action, context):
    data = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']
    assert len(data['mappings']) == 1, data
    mapping = clone(data['mappings'][0])
    mapping.update({'action': action, 'key': 'Escape', 'triggers': [], 'modifiers': [],
                    'settingBehavior': 'InheritSettingsFromAction',
                    'playerMappableKeySettings': 'None'})
    # UE 프로퍼티 에디터는 기존 요소 변경과 ArrayAdd를 같은 쓰기로 받으면
    # 삽입 위치를 판별할 수 없다. 기존 1개를 먼저 교체·재조회한 뒤 추가한다.
    data['mappings'] = [mapping]
    setp(context, {'defaultKeyMappings': data})
    data = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']
    assert len(data['mappings']) == 1 and data['mappings'][0] == mapping, data
    extra = clone(data['mappings'][0])
    extra['key'] = 'F10'
    data['mappings'].append(extra)
    setp(context, {'defaultKeyMappings': data})
    result = validate_input()
    result['saved'] = save([ACTION, CONTEXT])
    return result


def finish_input():
    """첫 실행의 복제·IA 설정 후 IMC 배열 추가가 실패한 상태만 복구한다."""
    editable([ACTION, CONTEXT])
    action, context = asset(ACTION), asset(CONTEXT)
    values = getp(action, ['valueType', 'triggers', 'modifiers', 'playerMappableKeySettings'])
    assert values['valueType'] == 'Boolean' and values['triggers'] == [] and values['modifiers'] == [], values
    assert values['playerMappableKeySettings'] in (None, '', 'None') or (
        isinstance(values['playerMappableKeySettings'], dict)
        and not values['playerMappableKeySettings'].get('refPath')), values
    mappings = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']['mappings']
    if len(mappings) == 2:
        # 두 번째 쓰기까지 성공했으면 검증·지정 패키지 저장만 한다.
        result = validate_input()
        result['saved'] = save([ACTION, CONTEXT])
        return result
    assert len(mappings) == 1, mappings
    original = getp(asset('/Game/Game/Input/IMC_HubInteract'), ['defaultKeyMappings'])['defaultKeyMappings']['mappings']
    assert len(original) == 1, original
    assert mappings[0]['action'] in (original[0]['action'], action), mappings
    assert mappings[0]['key'] in (original[0]['key'], 'Escape'), mappings
    return configure_input_mappings(action, context)


def validate_input():
    action, context = asset(ACTION), asset(CONTEXT)
    values = getp(action, ['valueType', 'triggers', 'modifiers', 'playerMappableKeySettings'])
    assert values['valueType'] == 'Boolean' and values['triggers'] == [] and values['modifiers'] == [], values
    assert values['playerMappableKeySettings'] in (None, '', 'None') or (
        isinstance(values['playerMappableKeySettings'], dict)
        and not values['playerMappableKeySettings'].get('refPath')), values
    mappings = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']['mappings']
    assert len(mappings) == 2, mappings
    assert [mapping['key'] for mapping in mappings] == ['Escape', 'F10'], mappings
    for mapping in mappings:
        assert mapping['action'] == action and mapping['triggers'] == [] and mapping['modifiers'] == [], mapping
        assert mapping['settingBehavior'] == 'InheritSettingsFromAction', mapping
        assert mapping['playerMappableKeySettings'] in (None, '', 'None') or (
            isinstance(mapping['playerMappableKeySettings'], dict)
            and not mapping['playerMappableKeySettings'].get('refPath')), mapping
    return {'action': values, 'mappings': mappings}


def create_menu():
    assert_not_pie()
    assert not exists(MENU), ('Refusing replacement', MENU)
    asset(SETTINGS)
    styles = main_menu_styles()
    blueprint = tool(UMG + 'CreateWidgetBlueprint', folderPath=MENU.rsplit('/', 1)[0],
                     assetName='WBP_PauseMenu', parentClass=ref(PARENT))['returnValue']
    assert valid_ref(blueprint), blueprint
    entries = [entry for entry in info(blueprint)['widgets'] if actual_widget(entry)]
    if entries:
        assert len(entries) == 1 and entries[0]['widgetClassPath']['refPath'].endswith('CanvasPanel'), entries
        assert not valid_ref(entries[0].get('parent')), entries
        assert tool(UMG + 'RemoveWidget', widgetBlueprint=blueprint,
                    widget=entries[0]['widget'])['returnValue']
    root = umg(blueprint, 'Overlay', 'Overlay_Pause')
    setp(root['widget'], {'visibility': 'SelfHitTestInvisible'})
    background = umg(blueprint, 'Border', 'Border_Background', root['widget'])
    setp(background['widget'], {'brushColor': {'r': 0, 'g': 0, 'b': 0, 'a': 0.6},
                               'visibility': 'Visible'})
    overlay_slot(background)
    size = umg(blueprint, 'SizeBox', 'Size_Menu', root['widget'])
    setp(size['widget'], {'bOverride_WidthOverride': True, 'widthOverride': 420,
                         'visibility': 'SelfHitTestInvisible'})
    overlay_slot(size, center=True)
    menu = umg(blueprint, 'VerticalBox', 'Panel_Menu', size['widget'], variable=True)
    setp(menu['widget'], {'visibility': 'SelfHitTestInvisible'})
    setp(menu['slot'], {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Center'})
    title = add(blueprint, '/Script/CommonUI.CommonTextBlock', 'Text_Title', menu['widget'])
    copy_style(styles['title'], title['widget'], ['style', 'font', 'colorAndOpacity',
               'shadowColorAndOpacity', 'shadowOffset'])
    # 상태 문구를 참고하되 제목은 별도 글자 크기로 표시한다. 원본 스타일은 읽기만 한다.
    title_font = getp(title['widget'], ['font'])['font']
    title_font['size'] = 36
    setp(title['widget'], {'style': 'None', 'font': title_font})
    setp(title['widget'], {'text': '일시정지', 'justification': 'Center',
                          'visibility': 'HitTestInvisible'})
    setp(title['slot'], {'padding': margin(bottom=32), 'horizontalAlignment': 'HAlign_Fill'})
    for name, label in BUTTONS.items():
        box = umg(blueprint, 'SizeBox', 'Size_' + name, menu['widget'])
        setp(box['widget'], {'bOverride_HeightOverride': True, 'heightOverride': 58})
        setp(box['slot'], {'padding': margin(top=7, bottom=7), 'horizontalAlignment': 'HAlign_Fill'})
        button = add(blueprint, BUTTON, name, box['widget'], variable=True)
        if styles['button'] is not None and 'style' in available(styles['button']) and 'style' in available(button['widget']):
            copy_style(styles['button'], button['widget'], ['style'])
        setp(button['widget'], {'buttonText': label})
        setp(button['slot'], {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
    # 메뉴 묶음과 형제이므로 C++이 메뉴를 Collapsed로 해도 설정은 보인다.
    stack = add(blueprint, '/Script/CommonUI.CommonActivatableWidgetStack',
                'Stack_Panels', root['widget'], variable=True)
    setp(stack['widget'], {'visibility': 'SelfHitTestInvisible'})
    overlay_slot(stack, padding=margin(80, 40, 80, 40))
    # 모든 BindWidget을 만든 뒤 최초 컴파일한다.
    assert tool(UMG + 'CompileWidgetBlueprint', widgetBlueprint=blueprint)['returnValue'], MENU
    setp(blueprint, {'settingsPanelClass': generated(SETTINGS)})
    tool(BP + 'compile_blueprint', blueprint=blueprint, warnings_as_errors=True)
    result = validate_menu()
    result['saved'] = save([MENU])
    return result


def validate_menu():
    blueprint = asset(MENU)
    snapshot = info(blueprint)
    assert snapshot['info']['parentClass']['refPath'] == PARENT, snapshot['info']
    found = {}
    for name, suffix in BINDINGS.items():
        entry = require(snapshot['widgets'], name, suffix)
        assert entry['bIsVariable'], entry
        found[name] = entry
    root = require(snapshot['widgets'], 'Overlay_Pause')
    assert found['Stack_Panels']['parent'] == root['widget'], found['Stack_Panels']
    for name, label in BUTTONS.items():
        assert getp(found[name]['widget'], ['buttonText'])['buttonText'] == label, name
    assert getp(blueprint, ['settingsPanelClass'])['settingsPanelClass'] == generated(SETTINGS)
    background = require(snapshot['widgets'], 'Border_Background')
    color = getp(background['widget'], ['brushColor'])['brushColor']
    assert color['r'] == color['g'] == color['b'] == 0 and abs(color['a'] - 0.6) < 0.0001, color
    return {'bindings': found, 'settingsPanelClass': generated(SETTINGS), 'background': color}


def controller_defaults():
    return {'pauseMappingContext': asset(CONTEXT), 'pauseAction': asset(ACTION),
            'pauseMenuWidgetClass': generated(MENU)}


def connect_controllers():
    editable(CONTROLLERS)
    validate_input()
    validate_menu()
    desired = controller_defaults()
    before = {}
    # 두 Blueprint 모두 세 속성을 읽을 수 있는지 먼저 확인한다.
    for path in CONTROLLERS:
        before[path] = getp(cdo(asset(path)), list(desired))
    result = {}
    for path in CONTROLLERS:
        blueprint = asset(path)
        # 기존 미커밋 기본값을 보존하며 지정된 세 프로퍼티만 쓴다.
        setp(cdo(blueprint), desired)
        tool(BP + 'compile_blueprint', blueprint=blueprint, warnings_as_errors=True)
        after = getp(cdo(blueprint), list(desired))
        assert after == desired, (path, after, desired)
        result[path] = {'before': before[path], 'after': after}
    return {'controllers': result, 'saved': save(CONTROLLERS)}


def inspect_state():
    result = {}
    for path in [ACTION, CONTEXT, MENU] + CONTROLLERS:
        entry = {'exists': exists(path)}
        if entry['exists']:
            entry['dirty'] = tool(ASSET + 'is_dirty', asset_path=path)['returnValue']
        result[path] = entry
    return result


def validate_all():
    result = {'input': validate_input(), 'menu': validate_menu(), 'controllers': {}}
    desired = controller_defaults()
    for path in CONTROLLERS:
        actual_values = getp(cdo(asset(path)), list(desired))
        assert actual_values == desired, (path, actual_values, desired)
        result['controllers'][path] = actual_values
    result['packages'] = inspect_state()
    return result


def run_phase(phase):
    assert_not_pie()
    phases = {'inspect': inspect_state, 'input': create_input, 'finish_input': finish_input, 'ui': create_menu,
              'controllers': connect_controllers, 'validate': validate_all}
    assert phase in phases, ('Unknown phase', phase)
    return phases[phase]()


def run():
    return run_phase('inspect')
