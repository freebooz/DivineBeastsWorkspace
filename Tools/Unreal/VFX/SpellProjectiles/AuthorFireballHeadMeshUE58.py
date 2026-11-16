# -*- coding: utf-8 -*-
"""确定性OBJ几何源，经Monolith/UE原生导入为当前唯一圆滑球体；无权威碰撞。"""
from pathlib import Path
import json,math,hashlib
import unreal as u
ROOT=Path(__file__).resolve().parents[4]
ASSET='/DBAPresentationPack_Core/VFX/SpellProjectiles/Meshes/Fireball/SM_DBA_Fireball_CoreSphere'
OWN=ROOT/'Saved/Validation/FireballTrail20261101/HeadMesh.Ownership.json'
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/SpellProjectiles/Fireball/SM_DBA_Fireball_CoreSphere.obj'

def author_head_mesh():
    """64经线×32纬度100cm球体；几何源可审计，uasset只由原生导入器生成。"""
    assert '-SSLMFireballMaterialsOwned' in u.SystemLibrary.get_command_line()
    assert Path(u.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject'
    exists=u.EditorAssetLibrary.does_asset_exist(ASSET)
    if exists and not OWN.exists():raise RuntimeError('已有其他所有者')
    positions=[(0.,0.,50.)];rings=[];triangles=[]
    for lat in range(1,32):
        theta=math.pi*lat/32;ring=[]
        for lon in range(64):
            phi=2*math.pi*lon/64;ring.append(len(positions))
            positions.append((50*math.sin(theta)*math.cos(phi),50*math.sin(theta)*math.sin(phi),50*math.cos(theta)))
        rings.append(ring)
    bottom=len(positions);positions.append((0.,0.,-50.))
    for lon in range(64):
        nxt=(lon+1)%64;u0=lon/64;u1=(lon+1)/64
        triangles.append(([0,rings[0][lon],rings[0][nxt]],[((u0+u1)/2,0),(u0,1/32),(u1,1/32)]))
        for lat in range(30):
            a,b=rings[lat],rings[lat+1];v0=(lat+1)/32;v1=(lat+2)/32
            triangles.extend([([a[lon],b[lon],b[nxt]],[(u0,v0),(u0,v1),(u1,v1)]),([a[lon],b[nxt],a[nxt]],[(u0,v0),(u1,v1),(u1,v0)])])
        triangles.append(([rings[-1][lon],bottom,rings[-1][nxt]],[(u0,31/32),((u0+u1)/2,1),(u1,31/32)]))
    assert len(positions)==1986 and len(triangles)==3968
    lines=['# SSLM current fireball sphere - cm, smooth analytic normals','o FireballCoreSphere']
    lines+=['v %.9f %.9f %.9f'%v for v in positions]
    lines+=['vn %.9f %.9f %.9f'%tuple(c/50 for c in v) for v in positions]
    for ids,uvs in triangles:lines+=['vt %.9f %.9f'%uv for uv in uvs]
    lines.append('s 1')
    for n,(ids,uvs) in enumerate(triangles):lines.append('f '+' '.join(f'{i+1}/{n*3+j+1}/{i+1}' for j,i in enumerate(ids)))
    data=('\n'.join(lines)+'\n').encode('utf-8')
    if SOURCE.exists() and SOURCE.read_bytes()!=data and not OWN.exists():raise RuntimeError('几何源已有其他所有者')
    SOURCE.write_bytes(data)
    OWN.write_text(json.dumps({'asset':ASSET,'method':'native OBJ import','source':str(SOURCE),'sha256':hashlib.sha256(data).hexdigest()}),encoding='utf-8')
    options=u.FbxImportUI();options.set_editor_property('import_as_skeletal',False)
    options.set_editor_property('import_mesh',True);options.set_editor_property('import_materials',False);options.set_editor_property('import_textures',False)
    options.set_editor_property('automated_import_should_detect_type',False);options.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH)
    settings=options.get_editor_property('static_mesh_import_data')
    settings.set_editor_property('auto_generate_collision',False);settings.set_editor_property('generate_lightmap_u_vs',False)
    settings.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task=u.AssetImportTask();task.set_editor_property('filename',str(SOURCE));task.set_editor_property('destination_path',ASSET.rsplit('/',1)[0])
    task.set_editor_property('destination_name',ASSET.rsplit('/',1)[1]);task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',exists);task.set_editor_property('save',False);task.set_editor_property('options',options)
    task.set_editor_property('factory',u.FbxFactory())
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);u.AutomationLibrary.finish_loading_before_screenshot()
    mesh=u.load_asset(ASSET);assert isinstance(mesh,u.StaticMesh)
    mesh.set_material(0,u.load_asset('/DBAPresentationPack_Core/VFX/SpellProjectiles/Materials/Fireball/MI_DBA_Fireball_CoreSurface'))
    assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
    d=mesh.get_static_mesh_description(0);b=mesh.get_bounds()
    assert d.get_triangle_count()==3968
    assert all(abs(getattr(b.box_extent,k)-50)<.01 for k in ['x','y','z']),str(b)
    result={'asset':ASSET,'diameter_cm':100,'triangles':d.get_triangle_count(),'bounds':str(b),'source_sha256':hashlib.sha256(data).hexdigest(),'authority_collision_changed':False}
    (OWN.parent/'HeadMesh.Native.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result
