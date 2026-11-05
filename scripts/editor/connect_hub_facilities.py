"""Author the trial catalog, facility actors and gameplay UI via live Unreal MCP.

Run after compiling the Hub C++ classes and opening the saved L_Hub.
Never writes the original test Blueprint packages. Inspect tool schemas first.
"""
import json

UMG = 'UMGToolSet.UMGToolSet.'

def ref(path):
    return {'refPath':path}

def call(name,args):
    return execute_tool(name,json.dumps(args))

def exists(path):
    return call('editor_toolset.toolsets.asset.AssetTools.exists',{'path':path})['returnValue']

def duplicate(source,target):
    assert not exists(target), 'Refusing to overwrite '+target
    assert call('editor_toolset.toolsets.asset.AssetTools.duplicate',{'path':source,'new_path':target})['returnValue']
    return ref(target+'.'+target.split('/')[-1])

def properties(obj,names):
    return json.loads(call('editor_toolset.toolsets.object.ObjectTools.get_properties',{'instance':obj,'properties':names})['returnValue'])

def setp(obj,values):
    available=json.loads(call('editor_toolset.toolsets.object.ObjectTools.list_properties',{'instance':obj})['returnValue'])
    for name in values:
        assert name in available, 'Unknown property '+name+' on '+obj['refPath']
    properties(obj,list(values.keys()))
    assert call('editor_toolset.toolsets.object.ObjectTools.set_properties',{'instance':obj,'values':json.dumps(values)})['returnValue']

def spawn(cls,name,xyz,yaw=0):
    actor=call('editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_class',{
        'actor_type':ref('/Script/DIGETHER.'+cls),'name':name,'xform':{
            'location':dict(zip(['x','y','z'],xyz)),
            'rotation':{'pitch':0,'yaw':yaw,'roll':0}},'parent':None,'snap_to_ground':False})['returnValue']
    call('editor_toolset.toolsets.scene.SceneTools.set_actor_folder',{'actor':actor,'folder_path':'Hub/Gameplay'})
    return actor

def find(name):
    return call('editor_toolset.toolsets.scene.SceneTools.find_actors',{
        'root':None,'name':name,'actor_type':None,'tag':'','bounds':None,'collision_channels':[]})['returnValue']

def widgets(bp):
    return call(UMG+'GetWidgets',{'widgetBlueprint':bp})['returnValue']

def empty_ref(obj):
    return obj is None or obj == 'None' or (isinstance(obj,dict) and not obj.get('refPath'))

def add(bp,cls,name,parent):
    info=call(UMG+'AddWidget',{'widgetBlueprint':bp,'widgetClass':ref('/Script/UMG.'+cls),
        'widgetDisplayName':name,'parentWidget':parent,'childIndex':-1})['returnValue']
    if name.startswith('Text_') or name.startswith('Button_'):
        call(UMG+'ToggleWidgetAsVariable',{'widgetBlueprint':bp,'widget':info['widget'],'bIsVariable':True})
    return info

def canvas_slot(info,left,top,width,height,anchor=(0,0),alignment=(0,0)):
    setp(info['slot'],{'layoutData':{
        'offsets':{'left':left,'top':top,'right':width,'bottom':height},
        'anchors':{'minimum':{'x':anchor[0],'y':anchor[1]},'maximum':{'x':anchor[0],'y':anchor[1]}},
        'alignment':{'x':alignment[0],'y':alignment[1]}}})

def text(bp,name,parent,label,size=22,color=(.85,.88,.9)):
    result=add(bp,'TextBlock',name,parent)
    setp(result['widget'],{'text':label,'font':{'size':size},
        'colorAndOpacity':{'specifiedColor':dict(zip(['r','g','b','a'],list(color)+[1])),
                          'colorUseRule':'UseColor_Specified'},'autoWrapText':True})
    return result

def new_widget(name,parent_class):
    assert not exists('/Game/Game/UI/Hub/'+name)
    bp=call(UMG+'CreateWidgetBlueprint',{'folderPath':'/Game/Game/UI/Hub','assetName':name,
        'parentClass':ref('/Script/DIGETHER.'+parent_class)})['returnValue']
    tree=widgets(bp)
    roots=[w for w in tree['widgets'] if empty_ref(w.get('parent')) and empty_ref(w.get('namedSlotHost'))]
    root=roots[0]['widget'] if roots else add(bp,'CanvasPanel','Canvas_Root',None)['widget']
    return bp,root

def compile_widget(bp):
    assert call(UMG+'CompileWidgetBlueprint',{'widgetBlueprint':bp})['returnValue']

def build_panel():
    bp,root=new_widget('WBP_HubFacilityPanel','DigetherHubFacilityWidget')
    shade=add(bp,'Border','Border_Shade',root)
    setp(shade['widget'],{'brushColor':{'r':.005,'g':.008,'b':.012,'a':.72}})
    setp(shade['slot'],{'layoutData':{'anchors':{'minimum':{'x':0,'y':0},'maximum':{'x':1,'y':1}},
        'offsets':{'left':0,'top':0,'right':0,'bottom':0}}})
    panel=add(bp,'Border','Border_Panel',root)
    canvas_slot(panel,0,0,700,720,(.5,.5),(.5,.5))
    setp(panel['widget'],{'brushColor':{'r':.026,'g':.036,'b':.043,'a':1},
        'padding':{'left':0,'top':0,'right':0,'bottom':0}})
    body=add(bp,'CanvasPanel','Canvas_Content',panel['widget'])['widget']
    for name,label,size,color,y,h in [
        ('Text_Title','거점 시설',32,(.88,.65,.25),25,50),
        ('Text_Balance','내 잔고',21,(.8,.86,.89),84,35),
        ('Text_Status','시설을 확인하는 중입니다.',21,(.21,.82,.72),132,70),
        ('Text_Detail','',19,(.77,.81,.83),212,175)]:
        info=text(bp,name,body,label,size,color)
        canvas_slot(info,32,y,636,h)
    for index in range(1,4):
        button=add(bp,'Button','Button_Option'+str(index),body)
        canvas_slot(button,32,410+(index-1)*72,636,60)
        setp(button['widget'],{'backgroundColor':{'r':.12,'g':.24,'b':.25,'a':1}})
        label=text(bp,'Text_Option'+str(index),button['widget'],'선택',20,(.95,.95,.9))
        setp(label['widget'],{'justification':'Center'})
    close=add(bp,'Button','Button_Close',body)
    canvas_slot(close,448,648,220,44)
    label=text(bp,'Text_Close',close['widget'],'닫기',20,(.95,.92,.8))
    setp(label['widget'],{'justification':'Center'})
    compile_widget(bp)
    return bp

def build_result():
    bp,root=new_widget('WBP_HubRunResult','DigetherRunResultWidget')
    panel=add(bp,'Border','Border_Result',root)
    canvas_slot(panel,0,0,680,680,(.5,.5),(.5,.5))
    setp(panel['widget'],{'brushColor':{'r':.026,'g':.036,'b':.043,'a':1},
        'padding':{'left':0,'top':0,'right':0,'bottom':0}})
    body=add(bp,'CanvasPanel','Canvas_Result',panel['widget'])['widget']
    names=['Title','Outcome','GeneralCredits','HiddenCredits','DeathLoss','CorpseRefund','Payout','FinalBalance']
    for index,name in enumerate(names):
        line=text(bp,'Text_'+name,body,'탐사 결과' if index==0 else '',30 if index==0 else 22,
            (.88,.65,.25) if index==0 else (.83,.88,.89))
        canvas_slot(line,32,28+index*64,616,55)
    button=add(bp,'Button','Button_Close',body)
    canvas_slot(button,220,595,240,48)
    text(bp,'Text_Close',button['widget'],'거점으로 돌아가기',20)
    compile_widget(bp)
    return bp

def run():
    assert call('editor_toolset.toolsets.scene.SceneTools.get_current_level',{})['returnValue']=='/Game/Game/Maps/L_Hub'
    assert not find('Hub_Gameplay_State'), 'Hub facilities already connected'
    # New catalog and UI assets are guarded against overwriting existing work.
    catalog_path='/Game/Data/Hub/DA_HubTrialCatalog'
    assert not exists(catalog_path)
    catalog=call('editor_toolset.toolsets.data_asset.DataAssetTools.create',{
        'folder_path':'/Game/Data/Hub','asset_name':'DA_HubTrialCatalog',
        'asset_type':ref('/Script/DIGETHER.DigetherHubCatalog')})['returnValue']
    setp(catalog,{'offers':[
        {'itemId':'WoodBundle','displayName':'목재 묶음','priceCredits':10,'quantity':1},
        {'itemId':'Headlamp','displayName':'예비 헤드랜턴','priceCredits':20,'quantity':1},
        {'itemId':'Pickaxe','displayName':'시험 곡괭이','priceCredits':30,'quantity':1}],
        'stages':[
        {'stageId':'MineFree','displayName':'광산 시험 구역','unlockCostCredits':0,'bInitiallyUnlocked':True},
        {'stageId':'Ruins','displayName':'유적 시험 구역','unlockCostCredits':100,'bInitiallyUnlocked':False},
        {'stageId':'Crystal','displayName':'수정 시험 구역','unlockCostCredits':200,'bInitiallyUnlocked':False}],
        'offeringCostCredits':5,'offeringPulseSeconds':10,'trialStarterCredits':500})
    panel=build_panel()
    result=build_result()
    hud=duplicate('/Game/Game/UI/WBP_DigetherRunHud','/Game/Game/UI/Hub/WBP_HubRunHud')
    tree=widgets(hud)
    roots=[w for w in tree['widgets'] if empty_ref(w.get('parent')) and empty_ref(w.get('namedSlotHost'))]
    assert roots and 'CanvasPanel' in roots[0]['widgetClassPath']['refPath'], 'Expected existing Canvas HUD'
    hint=text(hud,'Text_Interaction',roots[0]['widget'],'시설을 바라보고 E',22,(.9,.76,.44))
    canvas_slot(hint,0,-85,1100,44,(.5,1),(.5,.5))
    setp(hint['widget'],{'justification':'Center','visibility':'HitTestInvisible'})
    compile_widget(hud)
    context=duplicate('/Game/Game/Input/IMC_DepositInteract_Test','/Game/Game/Input/IMC_HubInteract')
    action=duplicate('/Game/Game/Input/Actions/IA_DepositInteract_Test','/Game/Game/Input/Actions/IA_HubInteract')
    # UE5.8 uses DefaultKeyMappings; the deprecated Mappings array is empty.
    mappings=properties(context,['defaultKeyMappings'])['defaultKeyMappings']['mappings']
    assert mappings
    for mapping in mappings:
        mapping['action']=action
    setp(context,{'defaultKeyMappings':{'mappings':mappings}})
    controller=duplicate('/Game/Game/Blueprints/BP_DigetherGameplayPlayerController_Test','/Game/Game/Blueprints/BP_DigetherHubPlayerController')
    setp(controller,{'interactMappingContext':context,'interactAction':action,
        'hubFacilityWidgetClass':ref(panel['refPath']+'_C'),
        'hudWidgetClass':ref(hud['refPath']+'_C'),'runResultWidgetClass':ref(result['refPath']+'_C')})
    call('editor_toolset.toolsets.blueprint.BlueprintTools.compile_blueprint',{'blueprint':controller,'warnings_as_errors':False})
    mode=duplicate('/Game/Game/Blueprints/BP_DigetherGameplayGameMode_Test','/Game/Game/Blueprints/BP_DigetherHubGameMode')
    setp(mode,{'playerControllerClass':ref(controller['refPath']+'_C'),'bValidateSavedHubSpawn':True})
    call('editor_toolset.toolsets.blueprint.BlueprintTools.compile_blueprint',{'blueprint':mode,'warnings_as_errors':False})
    setp(ref('/Game/Game/Maps/L_Hub.L_Hub:PersistentLevel.WorldSettings'),{'defaultGameMode':ref(mode['refPath']+'_C')})
    state=spawn('DigetherHubStateActor','Hub_Gameplay_State',(0,0,0))
    pool_lights=find('Hub_PoolBounce')
    assert len(pool_lights)==1
    setp(state,{'catalog':catalog,'offeringLight':pool_lights[0]})
    facilities=[]
    for name,kind,position,yaw,rest in [
        ('HubShop','Shop',(-690,-80,80),180,None),
        ('HubDelivery','Delivery',(-650,350,80),180,None),
        ('HubBed1','Bed',(700,-190,85),0,{'x':140,'y':0,'z':55}),
        ('HubBed2','Bed',(700,150,85),0,{'x':140,'y':0,'z':55}),
        ('HubAltar','Altar',(0,-800,80),-90,None),
        ('HubStage','Stage',(250,300,100),90,None)]:
        facility=spawn('DigetherHubFacilityActor','Hub_Facility_'+name,position,yaw)
        values={'facilityId':name,'kind':kind,'interactionDistanceCm':250}
        if rest:values['restLocationOffset']=rest
        setp(facility,values)
        facilities.append(facility)
    elevator=ref('/Game/Game/Maps/L_Hub.L_Hub:PersistentLevel.DigetherElevatorActor_0')
    setp(ref(elevator['refPath']+'.DoorThresholdVolume'),{
        'relativeLocation':{'x':0,'y':-125,'z':100},'boxExtent':{'x':200,'y':50,'z':100}})
    setp(elevator,{'doorOutwardDirection':{'x':0,'y':-1,'z':0}})
    paths=[catalog['refPath'],panel['refPath'],result['refPath'],hud['refPath'],context['refPath'],
        action['refPath'],controller['refPath'],mode['refPath'],'/Game/Game/Maps/L_Hub']
    assert call('editor_toolset.toolsets.asset.AssetTools.save_assets',{'asset_paths':paths})['returnValue']
    return {'saved':paths,'state':state,'facilities':facilities}
