"""Local emissive and procedural pool parents; existing template parents stay untouched."""
import json

def call(name, args):
    return execute_tool(name, json.dumps(args))

def ref(path):
    return {'refPath':path}

def create(name):
    path='/Game/Game/Environment/Hub/Materials/'+name
    assert not call('editor_toolset.toolsets.asset.AssetTools.exists',{'path':path})['returnValue']
    return call('editor_toolset.toolsets.material.MaterialTools.create_material',{
        'folder_path':'/Game/Game/Environment/Hub/Materials','asset_name':name})['returnValue']

def node(material, cls, x, y, values):
    obj=call('editor_toolset.toolsets.material.MaterialTools.add_expression',{
        'material_or_function':material,'expression_class':ref('/Script/Engine.MaterialExpression'+cls),'x':x,'y':y})['returnValue']
    if values:
        assert call('editor_toolset.toolsets.object.ObjectTools.set_properties',{'instance':obj,'values':json.dumps(values)})['returnValue']
    return obj

def pins(obj):
    return call('editor_toolset.toolsets.material.MaterialTools.get_expression_input_names',{'expression':obj})['returnValue']

def finish(glow, color, water, nodes):
    def connect(source, target, pin):
        call('editor_toolset.toolsets.material.MaterialTools.connect_expressions',{
            'from_expression':nodes[source],'from_output_name':'',
            'to_expression':nodes[target],'to_input_name':pin})
    def output(source, prop):
        call('editor_toolset.toolsets.material.MaterialTools.connect_to_output',{
            'expression':source,'output_name':'','material_property':prop})
    output(color,'MP_EmissiveColor')
    for source,target,pin in [('time','drift','A'),('flow','drift','B'),
                              ('position','add','A'),('drift','add','B'),
                              ('add','noise','World Position'),('noise','power','Base'),
                              ('deep','lerp','A'),('shallow','lerp','B'),('power','lerp','Alpha'),
                              ('lerp','emission','A')]:
        connect(source,target,pin)
    for source, prop in [('lerp','MP_BaseColor'),('emission','MP_EmissiveColor'),
                         ('roughness','MP_Roughness'),('metallic','MP_Metallic')]:
        output(nodes[source],prop)
    for material in [glow,water]:
        call('editor_toolset.toolsets.material.MaterialTools.recompile',{'material_or_function':material})
    for name in ['OrangeGlow','TealGlow']:
        call('editor_toolset.toolsets.material_instance.MaterialInstanceTools.set_parent',{
            'instance':ref('/Game/Game/Environment/Hub/Materials/MI_Hub'+name+'.MI_Hub'+name),'parent':glow})
    water_instance=call('editor_toolset.toolsets.material_instance.MaterialInstanceTools.create',{
        'folder_path':'/Game/Game/Environment/Hub/Materials','asset_name':'MI_HubWater','parent':water})['returnValue']
    altars=call('editor_toolset.toolsets.scene.SceneTools.find_actors',{
        'root':None,'name':'Hub_Altar','actor_type':None,'tag':'','bounds':None,'collision_channels':[]})['returnValue']
    assert len(altars)==1, 'Expected exactly one Hub_Altar'
    assert call('editor_toolset.toolsets.object.ObjectTools.set_properties',{
        'instance':ref(altars[0]['refPath']+'.PoolSurface'),
        'values':json.dumps({'overrideMaterials':[water_instance]})})['returnValue']
    assert call('editor_toolset.toolsets.asset.AssetTools.save_assets',{
        'asset_paths':[glow['refPath'],water['refPath'],water_instance['refPath'],
            '/Game/Game/Environment/Hub/Materials/MI_HubOrangeGlow',
            '/Game/Game/Environment/Hub/Materials/MI_HubTealGlow']})['returnValue']
    return {'compiled':True,'water':water,'glow':glow}

def run():
    glow=create('M_HubEmissive')
    color=node(glow,'VectorParameter',-300,0,{'parameterName':'Color','defaultValue':{'r':1,'g':.3,'b':.05,'a':1}})
    water=create('M_HubWater')
    nodes={
        'position':node(water,'WorldPosition',-1000,0,{}),
        'time':node(water,'Time',-1000,200,{}),
        'flow':node(water,'Constant3Vector',-1000,350,{'constant':{'r':2,'g':1.2,'b':0,'a':1}}),
        'drift':node(water,'Multiply',-800,240,{}),
        'add':node(water,'Add',-650,0,{}),
        'noise':node(water,'Noise',-450,0,{'scale':.065,'quality':1,'noiseFunction':'NOISEFUNCTION_VoronoiALU','levels':1,'outputMin':0,'outputMax':1}),
        'power':node(water,'Power',-250,0,{'constExponent':4}),
        'deep':node(water,'VectorParameter',-400,300,{'parameterName':'DeepColor','defaultValue':{'r':.002,'g':.04,'b':.032,'a':1}}),
        'shallow':node(water,'VectorParameter',-400,450,{'parameterName':'ShallowColor','defaultValue':{'r':.025,'g':.5,'b':.32,'a':1}}),
        'lerp':node(water,'LinearInterpolate',0,0,{}),
        'emission':node(water,'Multiply',200,150,{'constB':.6}),
        'roughness':node(water,'ScalarParameter',0,550,{'parameterName':'Roughness','defaultValue':.17}),
        'metallic':node(water,'ScalarParameter',0,700,{'parameterName':'Metallic','defaultValue':.35})}
    for key, obj in nodes.items():
        pins(obj)
    return finish(glow,color,water,nodes)
