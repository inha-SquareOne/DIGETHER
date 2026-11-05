"""Configure only the duplicated L_Hub level using registered Unreal tools."""
import json

PREFIX = '/Game/Game/Maps/L_Hub.L_Hub:PersistentLevel.'

def ref(path):
    return {'refPath': path}

def get_level():
    return execute_tool('editor_toolset.toolsets.scene.SceneTools.get_current_level', '{}')['returnValue']

def find_actors(name):
    return execute_tool('editor_toolset.toolsets.scene.SceneTools.find_actors', json.dumps({
        'root':None,'name':name,'actor_type':None,'tag':'','bounds':None,
        'collision_channels':[]}))['returnValue']

def add_text_component(actor, name):
    return execute_tool('editor_toolset.toolsets.actor.ActorTools.add_component', json.dumps({
        'owner':actor,'component_type':ref('/Script/Engine.TextRenderComponent'),
        'name':name}))['returnValue']

def set_props(obj, values):
    assert execute_tool('editor_toolset.toolsets.object.ObjectTools.set_properties', json.dumps({
        'instance': obj, 'values': json.dumps(values)}))['returnValue'], values

def transform(actor, x, y, z, yaw=0):
    assert execute_tool('editor_toolset.toolsets.actor.ActorTools.set_actor_transform', json.dumps({
        'actor': actor, 'xform': {'location': {'x':x,'y':-y,'z':z},
        'rotation': {'pitch':0,'yaw':-yaw,'roll':0}, 'scale': {'x':1,'y':1,'z':1}}, 'worldspace':True}))['returnValue']

def spawn(cls, name, xyz):
    return execute_tool('editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_class', json.dumps({
        'actor_type':ref(cls), 'name':name, 'xform':{'location':{'x':xyz[0],'y':-xyz[1],'z':xyz[2]}},
        'parent':None,'snap_to_ground':False}))['returnValue']

def folder(actor, name):
    execute_tool('editor_toolset.toolsets.scene.SceneTools.set_actor_folder', json.dumps({'actor':actor,'folder_path':name}))

def remove(name):
    assert execute_tool('editor_toolset.toolsets.scene.SceneTools.remove_from_scene', json.dumps({'actor':ref(PREFIX+name)}))['returnValue']

def light(name, xyz, rgb, intensity, radius, shadows=False):
    actor=spawn('/Script/Engine.PointLight', name, xyz)
    component=ref(actor['refPath']+'.LightComponent0')
    set_props(component, {'mobility':'Movable','intensityUnits':'Unitless',
        'bUseInverseSquaredFalloff':False,'intensity':intensity,'attenuationRadius':radius,
        'lightFalloffExponent':2,'lightColor':dict(zip(['r','g','b','a'],rgb+[1])),
        'castShadows':shadows,'sourceRadius':12})
    folder(actor,'Hub/Lighting')
    return actor

def label(name, text, xyz, yaw, size):
    actor=spawn('/Script/Engine.Actor',name,xyz)
    component=add_text_component(actor,'Label')
    set_props(ref(actor['refPath']+'.DefaultSceneRoot'),{'mobility':'Static'})
    # Label yaws are engine directions; spawn alone converts plan-space Y.
    # ObjectTools exposes TextRenderColor as a normalized LinearColor.
    set_props(component,{'text':text,'worldSize':size,
        'horizontalAlignment':'EHTA_Center','verticalAlignment':'EVRTA_TextCenter',
        'textRenderColor':{'r':.9,'g':.65,'b':.26,'a':1},
        'relativeRotation':{'pitch':0,'yaw':yaw,'roll':0},
        'mobility':'Static','castShadow':False})
    folder(actor,'Hub/Signage')
    return actor

def run():
    assert get_level() == '/Game/Game/Maps/L_Hub'
    assert not find_actors('Hub_LanternLight_'), 'Hub lighting already exists; duplicate configuration stopped.'
    # These objects exist only in the new duplicate; the source test map is untouched.
    for name in ['StaticMeshActor_0','DirectionalLight_0']:
        remove(name)
    elevator=ref(PREFIX+'DigetherElevatorActor_0')
    transform(elevator,0,-575,0)
    set_props(ref(elevator['refPath']+'.BoardingVolume'),{
        'boxExtent':{'x':200,'y':125,'z':100},'relativeLocation':{'x':0,'y':0,'z':100}})
    set_props(ref(elevator['refPath']+'.FloorMesh'),{
        'relativeScale3D':{'x':4,'y':2.5,'z':.2},'bVisible':False,'bHiddenInGame':True})
    set_props(ref(elevator['refPath']+'.DoorThresholdVolume'),{
        'relativeLocation':{'x':0,'y':-125,'z':100},'boxExtent':{'x':200,'y':50,'z':100}})
    set_props(elevator,{'doorOutwardDirection':{'x':0,'y':-1,'z':0}})
    for index, x in [(0,-150),(1,150)]:
        lever=ref(PREFIX+'DigetherElevatorLeverActor_'+str(index))
        transform(lever,x,-590,0,90)
        set_props(lever,{'elevatorActor':elevator})
        set_props(ref(lever['refPath']+'.LeverMesh'),{'overrideMaterials':[
            ref('/Game/Game/Environment/Hub/Materials/MI_HubBrass.MI_HubBrass')]})
        folder(lever,'Hub/Gameplay')
    station=ref(PREFIX+'BP_DigetherDepositStation_Test_C_0')
    transform(station,0,520,0,-90)
    set_props(ref(station['refPath']+'.MachineMesh'),{'bVisible':False,'bHiddenInGame':True})
    folder(station,'Hub/Gameplay')
    folder(elevator,'Hub/Gameplay')
    for index, xyz in enumerate([(-160,-180,100),(160,-180,100),(-160,40,100),(160,40,100)]):
        actor=ref(PREFIX+'PlayerStart_'+str(index))
        transform(actor,*xyz,yaw=90)
        folder(actor,'Hub/Gameplay')
    lights=[]
    for i,(x,y) in enumerate([(-850,-190),(-850,320),(850,-300),(850,370),(-650,-400),(600,-400),(-650,400),(650,400),(-400,650),(400,650),(-400,1010),(400,1010),(-140,-640),(140,-640),(345,1250),(0,1450),(-345,1250)]):
        lights.append(light('Hub_LanternLight_'+str(i),(x,y,190),[1,.68,.38],2.5,580,i in [4,5]))
    for i,(x,y) in enumerate([(-430,0),(430,0),(0,700),(0,1210)]):
        lights.append(light('Hub_AmbientFill_'+str(i),(x,y,230),[.55,.65,.72],1.8,1200,False))
    lights.append(light('Hub_PoolBounce',(0,1060,110),[.05,1,.78],2.4,550,False))
    pp=spawn('/Script/Engine.PostProcessVolume','Hub_InteriorExposure',(0,0,0))
    set_props(pp,{'bUnbound':True,'settings':{
        'bOverride_AutoExposureMinBrightness':True,'autoExposureMinBrightness':0,
        'bOverride_AutoExposureMaxBrightness':True,'autoExposureMaxBrightness':0,
        'bOverride_AutoExposureBias':True,'autoExposureBias':0,
        'bOverride_BloomIntensity':True,'bloomIntensity':.22,
        'bOverride_AmbientOcclusionIntensity':True,'ambientOcclusionIntensity':.8,
        'bOverride_MotionBlurAmount':True,'motionBlurAmount':0,
        'bOverride_VignetteIntensity':True,'vignetteIntensity':.18}})
    folder(pp,'Hub/Lighting')
    signs=[]
    for name,text,xyz,yaw,size in [
        ('Hub_SignShop','SHOP',(-758,80,138),0,18),
        ('Hub_SignDelivery','DELIVERY',(-708,-350,132),0,15),
        ('Hub_SignStage','STAGE SELECT',(250,-343,158),-90,13),
        ('Hub_SignDeposit','QUOTA DEPOSIT',(0,528,87),90,11),
        ('Hub_SignElevator','ELEVATOR',(0,-445,228),-90,24),
        ('Hub_SignAltar','ALTAR',(0,1491,235),90,25)]:
        signs.append(label(name,text,xyz,yaw,size))
    return {'lights':lights,'postProcess':pp,'station':station,'elevator':elevator,'signs':signs}
