"""Connect settings input/audio assets through the live UE 5.8.4 MCP sandbox.

The editor operator owns package backups, locks and execution. Load this file in
ProgrammaticToolset and call one phase at a time; it has no import-time writes.
Config/DefaultGame.ini is returned as data and is not written by this script.
"""
import json

ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
BP = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
ACTOR = 'editor_toolset.toolsets.actor.ActorTools.'

VOICE_ACTION = '/Game/Game/Input/Actions/IA_PushToTalk'
VOICE_CONTEXT = '/Game/Game/Input/IMC_Voice'
MAPPABLE_CLASS = '/Script/EnhancedInput.PlayerMappableKeySettings'
CONTEXTS = [
    '/Game/Input/IMC_Default',
    '/Game/Input/IMC_MouseLook',
    '/Game/Game/Input/IMC_HubInteract',
    '/Game/Game/Input/IMC_Pour',
    VOICE_CONTEXT,
]
CONTROLLERS = [
    '/Game/Game/Blueprints/BP_DigetherHubPlayerController',
    '/Game/Game/Blueprints/BP_DigetherGameplayPlayerController_Test',
]
AUDIO_PATHS = {
    'volumeSoundMix': '/Game/Frontend/UI/Audio/SMix_UserVolume',
    'effectsSoundClass': '/Game/Frontend/UI/Audio/SC_Effects',
    'musicSoundClass': '/Game/Frontend/UI/Audio/SC_Music',
}
ACTION_LABELS = {
    '/Game/Input/Actions/IA_Move': ('Move', '이동', '이동'),
    '/Game/Input/Actions/IA_Jump': ('Jump', '점프', '이동'),
    '/Game/Game/Input/Actions/IA_HubInteract': ('HubInteract', '상호작용', '행동'),
    '/Game/Game/Input/Actions/IA_Pour': ('Pour', '액체 붓기', '행동'),
    VOICE_ACTION: ('PushToTalk', '누르고 말하기', '음성'),
}
MOVE_LABELS = {
    'W': ('MoveForward', '앞으로 이동'),
    'Up': ('MoveForwardArrow', '앞으로 이동(방향키)'),
    'S': ('MoveBackward', '뒤로 이동'),
    'Down': ('MoveBackwardArrow', '뒤로 이동(방향키)'),
    'A': ('MoveLeft', '왼쪽으로 이동'),
    'Left': ('MoveLeftArrow', '왼쪽으로 이동(방향키)'),
    'D': ('MoveRight', '오른쪽으로 이동'),
    'Right': ('MoveRightArrow', '오른쪽으로 이동(방향키)'),
}


def ref(path):
    return {'refPath': path}


def tool(tool_name, **arguments):
    # execute_tool raises RuntimeError for tool failures. Its sandbox result is
    # a strict mapping whose get() does not implement the Python default arg.
    return execute_tool(tool_name, json.dumps(arguments))


def clone(value):
    return json.loads(json.dumps(value))


def path_of(value):
    if isinstance(value, dict):
        return value['refPath']
    return '' if value in (None, '', 'None') else value


def package_of(value):
    return path_of(value).split('.', 1)[0]


def asset(path):
    assert tool(ASSET + 'exists', path=path)['returnValue'], path
    return ref(path + '.' + path.rsplit('/', 1)[-1])


def getp(obj, names):
    return json.loads(tool(OBJ + 'get_properties', instance=obj,
                           properties=names)['returnValue'])


def setp(obj, values):
    available = json.loads(tool(OBJ + 'list_properties', instance=obj)['returnValue'])
    assert all(name in available for name in values), (obj, list(values), available)
    # UE's property importer can ignore unknown nested fields: callers also
    # read back the specific fields and references that establish each phase.
    getp(obj, list(values))
    assert tool(OBJ + 'set_properties', instance=obj,
                values=json.dumps(values))['returnValue'], (obj, values)


def editable(paths):
    assert not tool('EditorToolset.EditorAppToolset.IsPIERunning')['returnValue']
    for path in paths:
        assert tool(ASSET + 'can_edit_asset', asset_path=path)['returnValue'], path


def save(paths):
    paths = list(dict.fromkeys(paths))
    assert paths, 'Never pass an empty list: save_assets would save all dirty assets'
    assert tool(ASSET + 'save_assets', asset_paths=paths)['returnValue'], paths
    return paths


def mapping_core(mappings):
    """Retain input semantics and ordering independently of settings metadata."""
    return [{name: clone(mapping[name]) for name in
             ['action', 'key', 'modifiers', 'triggers']} for mapping in mappings]


def inspect_inputs():
    result = {}
    for path in CONTEXTS:
        if tool(ASSET + 'exists', path=path)['returnValue']:
            context = asset(path)
            result[path] = getp(context, ['defaultKeyMappings'])
    return result


def create_voice_input():
    """Phase 1: duplicate known bool templates, replace only the new PTT input."""
    sources = [
        ('/Game/Game/Input/Actions/IA_HubInteract', VOICE_ACTION),
        ('/Game/Game/Input/IMC_HubInteract', VOICE_CONTEXT),
    ]
    editable([old for old, new in sources])
    for old, new in sources:
        asset(old)
        assert not tool(ASSET + 'exists', path=new)['returnValue'], 'Refusing to overwrite ' + new
    for old, new in sources:
        assert tool(ASSET + 'duplicate', path=old, new_path=new)['returnValue'], (old, new)
    action, context = asset(VOICE_ACTION), asset(VOICE_CONTEXT)
    assert getp(action, ['valueType'])['valueType'] == 'Boolean'
    # Duplicating after metadata was added must not reuse Interact's save key.
    setp(action, {'playerMappableKeySettings': 'None', 'triggers': [], 'modifiers': []})
    data = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']
    assert len(data['mappings']) == 1, data
    mapping = data['mappings'][0]
    mapping.update({'action': action, 'key': 'V', 'triggers': [], 'modifiers': [],
                    'settingBehavior': 'InheritSettingsFromAction',
                    'playerMappableKeySettings': 'None'})
    setp(context, {'defaultKeyMappings': data})
    actual = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']
    assert actual == data, (data, actual)
    assert getp(action, ['valueType', 'triggers', 'modifiers']) == {
        'valueType': 'Boolean', 'triggers': [], 'modifiers': []}
    return {'action': action, 'context': context, 'mapping': actual,
            'saved': save([VOICE_ACTION, VOICE_CONTEXT])}


def settings_values(settings):
    if not path_of(settings):
        return None
    return getp(ref(path_of(settings)), ['name', 'displayName', 'displayCategory'])


def has_name(values):
    return values is not None and values['name'] not in ('', 'None', None)


def fill_settings(settings, name, label, category, preserve_name=True):
    current = settings_values(settings)
    assert current is not None, settings
    desired = {}
    if not preserve_name or not has_name(current):
        desired['name'] = name
    if not current['displayName']:
        desired['displayName'] = label
    if not current['displayCategory']:
        desired['displayCategory'] = category
    if desired:
        setp(settings, desired)
    actual = settings_values(settings)
    for field in desired:
        assert actual[field] == desired[field], (settings, field, desired, actual)
    if preserve_name and has_name(current):
        assert actual['name'] == current['name'], (current, actual)
    assert has_name(actual) and actual['displayName'] and actual['displayCategory'], actual
    return actual


def action_metadata(path, name, label, category):
    action = asset(path)
    current = getp(action, ['playerMappableKeySettings'])['playerMappableKeySettings']
    if not path_of(current):
        # ReferenceConverter's CPF_PersistentInstance branch creates this class
        # with the input action as Outer. Then edit the returned subobject.
        setp(action, {'playerMappableKeySettings': ref(MAPPABLE_CLASS)})
        current = getp(action, ['playerMappableKeySettings'])['playerMappableKeySettings']
    assert path_of(current).startswith(path + '.'), current
    return fill_settings(ref(path_of(current)), name, label, category)


def connect_key_metadata():
    """Phase 2: expose keys without changing action/key/modifier/trigger order.

    Each keyboard mapping has its own name and First slot. The native UI,
    AssignWithSwap and ApplyKeyBindings consume First slots only, so hiding
    arrows in Second slots would bypass the user's key collision swap.
    Mouse/gamepad axis mappings remain in place and the C++ key UI filters them.
    Existing valid save names and nonempty display text are always preserved.
    """
    editable(CONTEXTS + list(ACTION_LABELS))
    before = inspect_inputs()
    assert set(before) == set(CONTEXTS), before
    # Read all existing movement override names before making any changes.
    move_path = '/Game/Input/Actions/IA_Move'
    plans = {}
    direction_names = {}
    for context_path in CONTEXTS:
        data = clone(before[context_path]['defaultKeyMappings'])
        entries = []
        for index, mapping in enumerate(data['mappings']):
            action_path = package_of(mapping['action'])
            if action_path == move_path and mapping['key'] in MOVE_LABELS:
                default_name, label = MOVE_LABELS[mapping['key']]
                old = settings_values(mapping['playerMappableKeySettings'])
                if mapping['settingBehavior'] == 'OverrideSettings' and has_name(old):
                    desired_name = old['name']
                else:
                    desired_name = default_name
                # An existing directional name must not silently merge another
                # direction. The operator can inspect this conflict safely.
                for other_direction, existing_name in direction_names.items():
                    assert other_direction == default_name or existing_name.lower() != desired_name.lower(), (
                        'Existing movement save names conflict', other_direction, default_name, desired_name)
                if default_name not in direction_names:
                    direction_names[default_name] = desired_name
                entries.append({'index': index, 'defaultName': default_name,
                                'name': desired_name, 'label': label})
        plans[context_path] = {'data': data, 'entries': entries}
    assert {'W', 'A', 'S', 'D'}.issubset({mapping['key'] for mapping in
            before['/Game/Input/IMC_Default']['defaultKeyMappings']['mappings']}), before
    actions = {}
    for path, labels in ACTION_LABELS.items():
        actions[path] = action_metadata(path, *labels)
    changes = []
    for context_path in CONTEXTS:
        plan = plans[context_path]
        if not plan['entries']:
            continue
        data = plan['data']
        for entry in plan['entries']:
            mapping = data['mappings'][entry['index']]
            mapping['settingBehavior'] = 'OverrideSettings'
            if not path_of(mapping['playerMappableKeySettings']):
                mapping['playerMappableKeySettings'] = ref(MAPPABLE_CLASS)
        context = asset(context_path)
        setp(context, {'defaultKeyMappings': data})
        actual = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']
        assert mapping_core(actual['mappings']) == mapping_core(
            before[context_path]['defaultKeyMappings']['mappings']), context_path
        for entry in plan['entries']:
            mapping = actual['mappings'][entry['index']]
            settings = ref(path_of(mapping['playerMappableKeySettings']))
            assert path_of(settings).startswith(context_path + '.'), settings
            old = settings_values(plan['data']['mappings'][entry['index']]['playerMappableKeySettings']) if (
                path_of(plan['data']['mappings'][entry['index']]['playerMappableKeySettings']) != MAPPABLE_CLASS) else None
            # Preserve every existing valid save name. New keyboard mappings
            # each receive a distinct name that the native UI can consume.
            name = entry['name'] if has_name(old) else direction_names[entry['defaultName']]
            values = fill_settings(settings, name, entry['label'], '이동')
            changes.append({'context': context_path, 'index': entry['index'],
                            'key': mapping['key'], 'settings': values})
    after = inspect_inputs()
    for path in CONTEXTS:
        assert mapping_core(after[path]['defaultKeyMappings']['mappings']) == mapping_core(
            before[path]['defaultKeyMappings']['mappings']), path
    return {'actions': actions, 'movement': changes,
            'mappingCorePreserved': CONTEXTS,
            'saved': save(list(ACTION_LABELS) + [path for path in CONTEXTS if plans[path]['entries']])}


def correct_direction_mapping_names():
    """Correct only the eight overrides created by this operation's first run.

    That run put arrows in hidden Second slots. Verify its exact metadata before
    changing the four arrow names; never rename unrelated or earlier save keys.
    Separate names make all eight movement keys First rows, allowing the native
    AssignWithSwap/ApplyKeyBindings path to detect and exchange their keys.
    """
    path = '/Game/Input/IMC_Default'
    editable([path])
    context = asset(path)
    before = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']
    previous = {
        'W': ('MoveForward', '앞으로 이동'),
        'Up': ('MoveForward', '앞으로 이동'),
        'S': ('MoveBackward', '뒤로 이동'),
        'Down': ('MoveBackward', '뒤로 이동'),
        'A': ('MoveLeft', '왼쪽으로 이동'),
        'Left': ('MoveLeft', '왼쪽으로 이동'),
        'D': ('MoveRight', '오른쪽으로 이동'),
        'Right': ('MoveRight', '오른쪽으로 이동'),
    }
    found = {}
    for mapping in before['mappings']:
        if package_of(mapping['action']) != '/Game/Input/Actions/IA_Move' or mapping['key'] not in previous:
            continue
        key = mapping['key']
        assert key not in found and mapping['settingBehavior'] == 'OverrideSettings', mapping
        obj = ref(path_of(mapping['playerMappableKeySettings']))
        assert path_of(obj).startswith(path + '.'), obj
        name, label = previous[key]
        values = settings_values(obj)
        assert values == {'name': name, 'displayName': label, 'displayCategory': '이동'}, (key, values)
        found[key] = {'object': obj, 'before': values}
    assert set(found) == set(previous), found
    # All guards above run before the first corrective write.
    for key in ['Up', 'Down', 'Left', 'Right']:
        name, label = MOVE_LABELS[key]
        setp(found[key]['object'], {'name': name, 'displayName': label})
    actual = {}
    for key in previous:
        name, label = MOVE_LABELS[key]
        values = settings_values(found[key]['object'])
        assert values == {'name': name, 'displayName': label, 'displayCategory': '이동'}, (key, values)
        actual[key] = values
    assert len({value['name'].lower() for value in actual.values()}) == 8, actual
    after = getp(context, ['defaultKeyMappings'])['defaultKeyMappings']
    assert mapping_core(after['mappings']) == mapping_core(before['mappings']), path
    return {'movement': actual, 'nativeFirstMovementRows': 8,
            'expectedTotalKeyRows': 12, 'mappingCorePreserved': path,
            'saved': save([path])}


def voice_component(blueprint):
    cdo = tool(BP + 'get_default_object', blueprint=blueprint)['returnValue']
    components = tool(ACTOR + 'get_components', actor=cdo,
                      component_type=ref('/Script/DIGETHER.DigetherVoiceComponent'))['returnValue']
    assert len(components) == 1, (blueprint, components)
    return components[0]


def connect_voice_controllers():
    """Phase 3: assign PTT assets on both native VoiceComponent defaults."""
    editable(CONTROLLERS)
    desired = {'pushToTalkAction': asset(VOICE_ACTION),
               'voiceMappingContext': asset(VOICE_CONTEXT)}
    controllers = {}
    # Complete both preflights before mutating either Blueprint.
    for path in CONTROLLERS:
        bp = asset(path)
        current = getp(voice_component(bp), list(desired))
        for name in desired:
            assert not path_of(current[name]) or current[name] == desired[name], (path, name, current)
    for path in CONTROLLERS:
        bp = asset(path)
        setp(voice_component(bp), desired)
        result = tool(BP + 'compile_blueprint', blueprint=bp, warnings_as_errors=True)
        actual = getp(voice_component(bp), list(desired))
        assert actual == desired, (path, actual, desired)
        controllers[path] = {'defaults': actual, 'compile': result}
    return {'controllers': controllers, 'saved': save(CONTROLLERS)}


def audio_properties(is_mix):
    if is_mix:
        return ['bApplyEQ', 'eQPriority', 'eQSettings', 'soundClassEffects',
                'initialDelay', 'fadeInTime', 'duration', 'fadeOutTime']
    return ['properties', 'childClasses', 'passiveSoundMixModifiers',
            'parentClass', 'assetUserData']


def engine_audio_snapshot():
    """Catch SoundClass editor callbacks accidentally altering engine assets."""
    paths = [
        '/Engine/EngineSounds/Duck_Normal_Attack_MixMod',
        '/Engine/EngineSounds/Music', '/Engine/EngineSounds/Master',
        '/Engine/EngineSounds/SFX', '/Engine/EngineSounds/Normal_Attack',
        '/Engine/EngineSounds/Special_Attack_PSM', '/Engine/EngineSounds/Voice',
    ]
    result = {}
    for path in paths:
        result[path] = {
            'values': getp(asset(path), audio_properties(path.endswith('_MixMod'))),
            'dirty': tool(ASSET + 'is_dirty', asset_path=path)['returnValue'],
        }
    return result


def verify_neutral_audio(mix, effects, music):
    values = getp(mix, audio_properties(True))
    assert not values['bApplyEQ'] and values['eQPriority'] == 0, values
    assert values['soundClassEffects'] == [], values
    assert values['initialDelay'] == 0 and values['duration'] == -1, values
    assert abs(values['fadeInTime'] - .2) < .000001, values
    assert abs(values['fadeOutTime'] - .2) < .000001, values
    for index, frequency in enumerate([600, 1000, 2000, 10000]):
        assert values['eQSettings']['frequencyCenter' + str(index)] == frequency, values
        assert values['eQSettings']['gain' + str(index)] == 1, values
        assert values['eQSettings']['bandwidth' + str(index)] == 1, values
    classes = {}
    for obj, is_music in [(effects, False), (music, True)]:
        data = getp(obj, audio_properties(False))
        assert data['childClasses'] == [] and data['passiveSoundMixModifiers'] == [], data
        assert not path_of(data['parentClass']) and data['assetUserData'] == [], data
        props = data['properties']
        assert props['volume'] == 1 and props['pitch'] == 1, props
        assert props['lowPassFilterFrequency'] == 20000 and props['attenuationDistanceScale'] == 1, props
        assert props['bIsMusic'] == is_music, props
        assert not props['bAlwaysPlay'] and not props['bIsUISound'], props
        assert not props['bApplyEffects'] and not props['bApplyAmbientVolumes'], props
        assert not path_of(props['defaultSubmix']), props
        classes[path_of(obj)] = data
    return {'mix': values, 'classes': classes}


def create_audio_assets():
    """Phase 4: create neutral local controls without inherited engine routing.

    Both SoundClasses deliberately duplicate the childless Music template.
    Resetting SFX.ChildClasses would invoke USoundClass editor callbacks that
    detach and dirty the referenced engine children. Only the new packages are
    reset; reset_properties obtains their native class defaults through UE.
    """
    editable([])
    mix_source = '/Engine/EngineSounds/Duck_Normal_Attack_MixMod'
    class_source = '/Engine/EngineSounds/Music'
    sources = [(mix_source, AUDIO_PATHS['volumeSoundMix']),
               (class_source, AUDIO_PATHS['effectsSoundClass']),
               (class_source, AUDIO_PATHS['musicSoundClass'])]
    before = engine_audio_snapshot()
    assert before[class_source]['values']['childClasses'] == [], before[class_source]
    for old, new in sources:
        assert not tool(ASSET + 'exists', path=new)['returnValue'], 'Refusing to overwrite ' + new
    for old, new in sources:
        assert tool(ASSET + 'duplicate', path=old, new_path=new)['returnValue'], (old, new)
    created = {}
    for old, new in sources:
        obj = asset(new)
        is_mix = new == AUDIO_PATHS['volumeSoundMix']
        actual_class = tool(OBJ + 'get_class', instance=obj)['returnValue']
        assert path_of(actual_class) == ('/Script/Engine.SoundMix' if is_mix else '/Script/Engine.SoundClass'), actual_class
        assert tool(OBJ + 'reset_properties', instance=obj,
                    properties=audio_properties(is_mix))['returnValue'], new
        created[new] = obj
    mix = created[AUDIO_PATHS['volumeSoundMix']]
    effects = created[AUDIO_PATHS['effectsSoundClass']]
    music = created[AUDIO_PATHS['musicSoundClass']]
    props = getp(music, ['properties'])['properties']
    props['bIsMusic'] = True
    setp(music, {'properties': props})
    actual = verify_neutral_audio(mix, effects, music)
    assert engine_audio_snapshot() == before, 'Engine audio source values or dirty state changed'
    saved = save(list(AUDIO_PATHS.values()))
    assert engine_audio_snapshot() == before, 'Engine audio state changed during save'
    return {'assets': created, 'values': actual,
            'engineSourcesUnchanged': list(before), 'saved': saved}


def options_config_paths():
    """Return canonical object paths for the operator's scoped Config edit."""
    return {
        'section': '/Script/DIGETHER.DigetherOptionsSettings',
        'RebindableMappingContexts': [asset(path)['refPath'] for path in CONTEXTS],
        'VolumeSoundMix': asset(AUDIO_PATHS['volumeSoundMix'])['refPath'],
        'EffectsSoundClass': asset(AUDIO_PATHS['effectsSoundClass'])['refPath'],
        'MusicSoundClass': asset(AUDIO_PATHS['musicSoundClass'])['refPath'],
    }
