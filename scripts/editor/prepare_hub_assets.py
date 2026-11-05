"""Run through Unreal ProgrammaticToolset after opening the new L_Hub map."""
import json

def call(name, args):
    return execute_tool(name, json.dumps(args))

def exists(path):
    return call('editor_toolset.toolsets.asset.AssetTools.exists', {'path': path})['returnValue']

def import_mesh(name):
    path = '/Game/Game/Environment/Hub/Meshes/' + name
    if exists(path):
        return {'refPath': path + '.' + name}
    result = call('editor_toolset.toolsets.static_mesh.StaticMeshTools.import_file', {
        'folder_path': '/Game/Game/Environment/Hub/Meshes', 'asset_name': name,
        'source_file': 'D:/DIGETHER/DIGETHER/SourceArt/Hub/' + name + '.obj',
        'import_materials': False, 'import_textures': False, 'combine_meshes': True})['returnValue']
    assert len(result) == 1, result
    return result[0]

def bounds(mesh):
    return call('editor_toolset.toolsets.static_mesh.StaticMeshTools.get_bounds', {'mesh': mesh})['returnValue']

def collision(mesh):
    assert call('editor_toolset.toolsets.static_mesh.StaticMeshTools.generate_convex_collisions', {
        'mesh': mesh, 'hull_count': 1, 'max_hull_verts': 64, 'hull_precision': 100000})['returnValue']

def instance(name, color, metallic=0, roughness=.85, glow=False):
    path = '/Game/Game/Environment/Hub/Materials/MI_Hub' + name
    assert not exists(path), 'Refusing to overwrite: ' + path
    parent = '/Game/LevelPrototyping/Interactable/JumpPad/Assets/Materials/M_SimpleGlow.M_SimpleGlow' if glow else '/Game/LevelPrototyping/Materials/M_FlatCol.M_FlatCol'
    material = call('editor_toolset.toolsets.material_instance.MaterialInstanceTools.create', {
        'folder_path': '/Game/Game/Environment/Hub/Materials', 'asset_name': 'MI_Hub' + name,
        'parent': {'refPath': parent}})['returnValue']
    call('editor_toolset.toolsets.material_instance.MaterialInstanceTools.set_vector_parameter', {
        'instance': material, 'name': 'Color' if glow else 'Base Color',
        'value': dict(zip(['r','g','b','a'], color + [1]))})
    if not glow:
        for parameter, value in [('Metallic', metallic), ('Roughness', roughness)]:
            call('editor_toolset.toolsets.material_instance.MaterialInstanceTools.set_scalar_parameter', {
                'instance': material, 'name': parameter, 'value': value})
    return path

def run():
    meshes = {}
    for name in ['SM_HubBeveledCube', 'SM_HubHalfDiscFloor', 'SM_HubPoolRim', 'SM_HubDome', 'SM_HubPoolDisc']:
        mesh = import_mesh(name)
        meshes[name] = bounds(mesh)
        if name in ['SM_HubBeveledCube', 'SM_HubHalfDiscFloor']:
            collision(mesh)
    materials = []
    for name, color in [('StoneA',[.16,.135,.11]), ('StoneB',[.13,.11,.09]), ('StoneC',[.19,.16,.13]),
                        ('FloorA',[.12,.10,.085]), ('FloorB',[.105,.088,.075])]:
        materials.append(instance(name, color))
    materials.append(instance('Brass', [.4,.22,.055], .72, .38))
    materials.append(instance('DarkMetal', [.035,.04,.044], .65, .5))
    materials.append(instance('Wood', [.115,.052,.023], 0, .8))
    materials.append(instance('Bed', [.09,.16,.14], 0, .95))
    materials.append(instance('TealGlow', [.015,1.4,1.0], glow=True))
    materials.append(instance('OrangeGlow', [6,1.8,.08], glow=True))
    paths = materials + ['/Game/Game/Environment/Hub/Meshes/' + n for n in meshes]
    assert call('editor_toolset.toolsets.asset.AssetTools.save_assets', {'asset_paths': paths})['returnValue']
    return {'meshes': meshes, 'materials': materials}
