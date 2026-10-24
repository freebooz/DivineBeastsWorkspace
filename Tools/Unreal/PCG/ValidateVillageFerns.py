"""Monolith只读原生审查：源图/材质、真实实例与空间排除。不是GPU或Cook验收。"""
import hashlib
import json
import math
import re
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve().parent
OUT=ROOT/'Saved/Validation/VillageFerns20261024'


def run():
    sources=json.loads((OUT/'SourceManifest.json').read_text(encoding='utf-8-sig'))
    for source in sources:
        assert hashlib.sha256(Path(source['Copy']).read_bytes()).hexdigest().upper()==source['SHA256']
    library='/DBAFoliagePack_Core/Foliage'
    paths=list(u.EditorAssetLibrary.list_assets(library+'/Grass',True,False))
    paths += [library+'/FoliageTypes/FT_DBA_'+s for s in ['BostonFern','WesternSwordFern']]
    paths += ['/DBAWorldPack_Village/PCG/Ferns/'+p for p in ['PCG_DBA_Village_Ferns','BP_DBA_VillageFernScatter']]
    validator=u.get_editor_subsystem(u.EditorValidatorSubsystem)
    validation=[];meshes=[]
    for p in paths:
        a=u.load_asset(str(p));assert a
        result,errors,warnings=validator.is_asset_valid(u.EditorAssetLibrary.find_asset_data(str(p)),u.DataValidationUsecase.MANUAL)
        error_text=[str(e) for e in errors]
        # 既有校验器将FoliageType_InstancedStaticMesh的类名子串误判为StaticMesh。
        # 保留原始失败，不为通过改成错误SM_前缀，也不跳过其它原生错误。
        known_naming=bool(error_text) and isinstance(a,u.FoliageType_InstancedStaticMesh) and all(
            'GP.Naming' in e and 'SM_' in e for e in error_text)
        assert not error_text or known_naming,(str(p),error_text)
        validation.append({'path':str(p),'result':str(result),'errors':error_text,
            'knownFoliageTypeNamingMisclassification':known_naming,'warnings':[str(w) for w in warnings]})
        if isinstance(a,u.Texture2D):
            assert a.get_editor_property('srgb')==(not a.get_name().endswith('_Normal')),a.get_name()
        if isinstance(a,u.StaticMesh):
            meshes.append({'path':a.get_path_name(),'trianglesLOD0':a.get_num_triangles(0),'lods':a.get_num_lods()})
        if isinstance(a,u.MaterialInstanceConstant):
            assert a.get_editor_property('parent').get_path_name().startswith('/SpeedTreeImporter/SpeedTree9/')
            if 'Fiddlehead' not in a.get_name():
                for key in ['ColorOpacity','NormalRoughness']:
                    tex=u.MaterialEditingLibrary.get_material_instance_texture_parameter_value(a,key)
                    assert isinstance(tex,u.Texture2D) and tex.get_path_name().startswith(library+'/Grass/'),(a.get_name(),key)
    actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    a=next(a for a in actors if a.get_actor_label()=='Village_Ferns_PCG')
    assert a.get_editor_property('pcg_component').generation_trigger==u.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND
    director=next(a for a in actors if isinstance(a,u.GamePlatformPCGWorldDirector))
    masks,error=director.collect_spatial_masks();assert not error
    def distance(p,a,b):
        vx,vy=b.x-a.x,b.y-a.y
        t=max(0.,min(1.,((p.x-a.x)*vx+(p.y-a.y)*vy)/max(vx*vx+vy*vy,1e-9)))
        return math.hypot(p.x-a.x-t*vx,p.y-a.y-t*vy)
    def inside(p,m):
        vertices=m.vertices;closed=m.get_editor_property('closed')
        # C++ bClosed在Python中暴露为closed。
        n=len(vertices);hit=False
        if closed and m.get_editor_property('fill_interior'):
            j=n-1
            for i in range(n):
                vi,vj=vertices[i],vertices[j]
                if (vi.y>p.y)!=(vj.y>p.y) and p.x<(vj.x-vi.x)*(p.y-vi.y)/(vj.y-vi.y)+vi.x:hit=not hit
                j=i
        if hit:return True
        edges=n if closed else n-1
        return any(distance(p,vertices[i],vertices[(i+1)%n])<=m.half_width_cm for i in range(edges))
    checked=0
    for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
        for i in range(c.get_instance_count()):
            p=c.get_instance_transform(i,world_space=True).translation
            for m in masks:
                if m.priority>30 and m.strength>=.999:
                    assert not inside(p,m),(i,m.domain_id,str(p))
            checked+=1
    naming_issues=sum(bool(v['errors']) for v in validation)
    report={'passed':naming_issues==0,'contentAndSpatialIntegrityPassed':True,
        'existingValidatorNamingIssues':naming_issues,'sourceFilesSHA256Checked':len(sources),'nativeAssets':validation,'meshes':meshes,
        'instancesCheckedAgainstWorldDirectorMasks':checked,'maskCount':len(masks),
        'runtimeGeneration':'OnDemand/editor-baked','cookTested':False,'gpuMeasured':False}
    (OUT/'NativeValidation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('FERN_CONTENT_SPATIAL_PASS',len(paths),len(sources),checked,len(masks),'EXISTING_NAMING_ISSUES',naming_issues,meshes)


run()
