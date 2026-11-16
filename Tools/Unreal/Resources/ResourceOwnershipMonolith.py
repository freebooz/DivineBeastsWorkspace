"""正式UE工程原生资源归属作者；只从有限合同操作，所有证据在工程Saved。"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parent
OUT = ROOT/'Saved/Validation/ResourceOwnershipRemediation20261031'
spec = importlib.util.spec_from_file_location('resource_contract', Path(__file__).with_name('ResourceOwnershipContract.py'))
contract = importlib.util.module_from_spec(spec); spec.loader.exec_module(contract)
DOC = contract.load_contract()
MOVES = contract.validate_contract(DOC)
REG = u.AssetRegistryHelpers.get_asset_registry()
OPTIONS = u.AssetRegistryDependencyOptions(include_hard_package_references=True,
    include_soft_package_references=True, include_hard_management_references=True,
    include_soft_management_references=True, include_searchable_names=False,
    include_game_package_references=True, include_editor_only_package_references=True)


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')


def context(clean=True):
    assert str(u.Paths.get_project_file_path()).replace('\\','/').endswith('/Game/DivineBeastsArena.uproject')
    assert not u.EditorLevelLibrary.get_pie_worlds(False), '迁移禁止PIE在途'
    dirty = [p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    dirty += [p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    if clean: assert not dirty, '存在未保存人工修改，拒绝覆盖：'+str(dirty)
    assert (OUT/'BackupInventory.json').is_file(), '先完成原资产字节备份'
    return dirty


def normalized(value):
    """仅用于保效对照，把新位置还原为同一旧身份；不改实际UObject或资产。"""
    if isinstance(value, str):
        for move in sorted(MOVES, key=lambda m:len(m['target']), reverse=True):
            value = value.replace(move['target'],move['source'])
        return value
    if isinstance(value, dict): return {k:normalized(v) for k,v in value.items()}
    if isinstance(value, (list,tuple)): return [normalized(x) for x in value]
    return value


def path(obj): return obj.get_path_name() if obj else None


def export(obj, filename, exporter):
    filename.parent.mkdir(parents=True, exist_ok=True)
    assert not filename.exists(), '不覆盖已有原生导出'
    task=u.AssetExportTask();task.object=obj;task.filename=str(filename)
    task.automated=True;task.prompt=False;task.replace_identical=False;task.exporter=exporter
    assert u.Exporter.run_asset_export_task(task), '原生导出失败：'+str(filename)
    return hashlib.sha256(filename.read_bytes()).hexdigest()


def appearance(obj, source, stage):
    """真实像素、网格和材质状态；不修改颜色、缩放或任何图节点。"""
    result={'class':obj.get_class().get_path_name(),'leaf':obj.get_name()}
    folder=OUT/'Exports'/stage/hashlib.sha256(source.encode()).hexdigest()[:12]
    if isinstance(obj,u.Texture2D):
        result.update(size=[obj.blueprint_get_size_x(),obj.blueprint_get_size_y()],
            srgb=obj.get_editor_property('srgb'),compression=str(obj.get_editor_property('compression_settings')),
            pixelPNG=export(obj,folder/(obj.get_name()+'.png'),u.TextureExporterPNG()))
    elif isinstance(obj,u.StaticMesh):
        count=u.EditorStaticMeshLibrary.get_lod_count(obj)
        result['lodVertices']=[u.EditorStaticMeshLibrary.get_number_verts(obj,i) for i in range(count)]
        result['bounds']=obj.get_bounds().export_text()
        result['materials']=[{'slot':str(s.material_slot_name),'material':path(s.material_interface)} for s in obj.get_editor_property('static_materials')]
        # OBJ是原生几何旁证，路径/材质文件名不作为几何相等结论。
        result['geometryOBJ']=export(obj,folder/(obj.get_name()+'.obj'),u.StaticMeshExporterOBJ())
    elif isinstance(obj,u.MaterialInterface):
        ed=u.MaterialEditingLibrary
        instance=isinstance(obj,u.MaterialInstanceConstant)
        result['parent']=path(obj.get_editor_property('parent')) if instance else None
        values={}
        for kind in ('scalar','vector','texture','static_switch'):
            names=getattr(ed,'get_'+kind+'_parameter_names')(obj)
            getter=getattr(ed,'get_material_instance_'+kind+'_parameter_value' if instance else 'get_material_default_'+kind+'_parameter_value')
            items={}
            for name in names:
                v=getter(obj,name)
                items[str(name)]=path(v) if kind=='texture' else (v.export_text() if hasattr(v,'export_text') else v)
            values[kind]=items
        result['parameters']=values
        if not instance:
            result['domain']=str(obj.get_editor_property('material_domain'))
            result['blend']=str(obj.get_editor_property('blend_mode'))
            result['twoSided']=obj.get_editor_property('two_sided')
    return normalized(result)


def baseline():
    context()
    assert not (OUT/'Baseline.json').exists(), '保留已有原生基线，不重写'
    real=set();targets=set()
    for move in MOVES:
        source=u.load_asset(move['source'],follow_redirectors=False)
        assert source and source.get_class().get_path_name()!='/Script/CoreUObject.ObjectRedirector', move['source']
        real.add(move['source'])
        if u.EditorAssetLibrary.does_asset_exist(move['target']):targets.add(move['target'])
    contract.preflight_paths(DOC,real,targets)
    records=[]
    for move in MOVES:
        obj=u.load_asset(move['source'],follow_redirectors=False)
        records.append({'source':move['source'],'target':move['target'],'group':move['group'],
            'appearance':appearance(obj,move['source'],'Before'),
            'deps':sorted(set(str(x) for x in REG.get_dependencies(move['source'],OPTIONS))),
            'refs':sorted(set(str(x) for x in REG.get_referencers(move['source'],OPTIONS)))})
    context()
    write('Baseline.json',{'pid':os.getpid(),'records':records})
    print('NATIVE_BASELINE_SAVED',len(records))


def move_group(group):
    context()
    baseline=json.loads((OUT/'Baseline.json').read_text(encoding='utf-8'))
    known={r['source']:r for r in baseline['records']}
    items=[m for m in MOVES if m['group']==group]
    assert items, '未知迁移组'
    assert not (OUT/('Moved.'+group+'.json')).exists(), '已有组结果，禁止重复迁移'
    renames=[]
    for m in items:
        assert not u.EditorAssetLibrary.does_asset_exist(m['target']), '目标已存在，不覆盖：'+m['target']
        obj=u.load_asset(m['source'],follow_redirectors=False)
        assert obj and obj.get_class().get_path_name()==known[m['source']]['appearance']['class']
        directory,_,leaf=m['target'].rpartition('/')
        renames.append(u.AssetRenameData(asset=obj,new_package_path=directory,new_name=leaf))
    accepted=u.AssetToolsHelpers.get_asset_tools().rename_assets(renames)
    results=[]
    for m in items:
        obj=u.load_asset(m['target'],follow_redirectors=False)
        results.append({'source':m['source'],'target':m['target'],'actual':path(obj),
                        'class':obj.get_class().get_path_name() if obj else None})
    write('Moved.'+group+'.json',{'pid':os.getpid(),'accepted':accepted,'results':results})
    assert accepted and all(r['actual']==r['target']+'.'+r['target'].rsplit('/',1)[1] for r in results), '部分迁移失败，保留结果，不重试覆盖'
    # RenameAssets拥有的真实引用已被引擎修改；入口脏包为0，保存本次留下的变更。
    # RenameAssets已保存目标及引用。空旧包可能仍标脏，强制FullyLoad再保存会复活旧磁盘对象。
    # 只保存有注册导出的非源脏包；源包由引擎迁移/后续重定向清理负责。
    sources={m['source'] for m in items}
    packages=[p for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()+u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        if p.get_name() not in sources and REG.get_assets_by_package_name(p.get_name())]
    assert not packages or u.EditorLoadingAndSavingUtils.save_packages(packages,False), '本组原生保存失败'
    context()
    print('RESOURCE_GROUP_SAVED',group,len(items))


def allow_cancelled_group_retry(group):
    """仅为原生明确取消、零目标写入的批次解除一次重试；保留原失败日志。"""
    context()
    old=OUT/('Moved.'+group+'.json');previous=json.loads(old.read_text(encoding='utf-8'))
    assert previous['accepted'] is False and all(r['actual'] is None for r in previous['results'])
    for m in MOVES:
        if m['group']!=group:continue
        assert not u.EditorAssetLibrary.does_asset_exist(m['target'])
        obj=u.load_asset(m['source'],follow_redirectors=False)
        assert obj and obj.get_class().get_path_name()!='/Script/CoreUObject.ObjectRedirector'
    archive=OUT/('Moved.'+group+'.Cancelled.json');assert not archive.exists()
    old.rename(archive)


def verify(stage='After', appearance_check=True):
    context()
    baseline=json.loads((OUT/'Baseline.json').read_text(encoding='utf-8'))
    failures=[];records=[]
    for before in baseline['records']:
        obj=u.load_asset(before['target'],follow_redirectors=False)
        if not obj or obj.get_class().get_path_name()=='/Script/CoreUObject.ObjectRedirector':
            failures.append('缺少真实目标：'+before['target']);continue
        if appearance_check:
            after=appearance(obj,before['source'],stage)
            # OBJ字符串可能包含导出路径；真实顶点数、Bounds、材质槽单独严格对照。
            expected=contract.appearance_signature(before['appearance']);actual=contract.appearance_signature(after)
            if expected!=actual:failures.append({'asset':before['target'],'before':expected,'after':actual})
            records.append({'package':before['target'],'appearance':after})
    write('Verify.'+stage+'.json',{'pid':os.getpid(),'passed':not failures,'failures':failures,'records':records})
    assert not failures, str(failures[:2])
    print('RESOURCE_APPEARANCE_VERIFIED',stage,len(records))


def recover_resurrected_samples():
    """恢复旧空包被错误FullyLoad后产生的样例副本；只接受本轮已成功移动的40项。"""
    context(False)
    items=[m for m in MOVES if m['group']=='plants_samples']
    journal=json.loads((OUT/'Moved.plants_samples.json').read_text(encoding='utf-8'))
    assert journal['accepted'] and len(journal['results'])==len(items)
    baseline={r['source']:r for r in json.loads((OUT/'Baseline.json').read_text(encoding='utf-8'))['records']}
    exported=json.loads((OUT/'RecoveryTargetAppearance.json').read_text(encoding='utf-8'))
    # 已导出PNG与完整参数检查：唯一差异允许是驻留Mip大小，源PNG仍严格相同。
    assert len(exported['records'])==len(items)
    assert all(contract.appearance_signature(f['expected'])==contract.appearance_signature(f['actual']) for f in exported['failures'])
    owned={m['source'] for m in items}
    assert set(context(False))<=owned, '拒绝处理非本轮源脏包'
    recovered=[]
    for m in items:
        old=u.load_asset(m['source'],follow_redirectors=False)
        new=u.load_asset(m['target'],follow_redirectors=False)
        assert new and new.get_class().get_path_name()==baseline[m['source']]['appearance']['class']
        if not old or old.get_class().get_path_name()=='/Script/CoreUObject.ObjectRedirector':continue
        assert old!=new and old.get_class()==new.get_class()
        assert u.EditorAssetLibrary.consolidate_assets(new,[old]), '原生引用收敛失败：'+m['source']
        alias=u.load_asset(m['source'],follow_redirectors=False)
        assert alias and alias.get_class().get_path_name()=='/Script/CoreUObject.ObjectRedirector'
        # Consolidate会替换Python对象包装；跨工具调用边界后另验重定向解析路径。
        recovered.append(m['source'])
    write('RecoveryConsolidated.json',{'pid':os.getpid(),'recovered':recovered})
    context()
    print('RECOVERED_NATIVE_UNIQUE_TARGETS',len(recovered))


def fix_references(groups):
    """已加载硬引用由原生Redirector解析；软引用显式交给AssetTools统一重写并保存。"""
    context()
    items=[m for m in MOVES if m['group'] in groups]
    sources={m['source'] for m in MOVES};refs=set();mapping={}
    for m in items:
        target=u.load_asset(m['target'],follow_redirectors=False)
        assert target and target.get_class().get_path_name()!='/Script/CoreUObject.ObjectRedirector'
        refs.update(str(x) for x in (REG.get_referencers(m['source'],OPTIONS) or []))
        old=m['source']+'.'+m['source'].rsplit('/',1)[1];new=path(target)
        mapping[u.SoftObjectPath(old)]=u.SoftObjectPath(new)
        if isinstance(target,u.Blueprint):mapping[u.SoftObjectPath(old+'_C')]=u.SoftObjectPath(new+'_C')
    objects=[];packages=[]
    for ref in sorted(refs-sources):
        obj=u.load_asset(ref,follow_redirectors=False)
        assert obj and obj.get_class().get_path_name()!='/Script/CoreUObject.ObjectRedirector', '引用包须是真实资产：'+ref
        objects.append(obj);packages.append(obj.get_outermost())
    u.AssetToolsHelpers.get_asset_tools().rename_referencing_soft_object_paths(packages,mapping)
    assert not objects or u.EditorAssetLibrary.save_loaded_assets(objects,False), '原生引用保存失败'
    write('ReferencesFixed.'+'-'.join(groups)+'.json',{'pid':os.getpid(),'packages':sorted(refs-sources)})
    context()
    print('NATIVE_REFERENCES_RESAVED',len(objects))


def cleanup_redirectors(groups=None, stage='Final'):
    """只清本轮已迁移源的无引用重定向；显式禁跟随，绝不按目录或模糊名删除。"""
    context()
    removed=[];retained=[]
    for m in MOVES:
        if groups is not None and m['group'] not in groups:continue
        obj=u.load_asset(m['source'],follow_redirectors=False)
        if not obj:continue
        assert obj.get_class().get_path_name()=='/Script/CoreUObject.ObjectRedirector', '源仍有真实资产，停止：'+m['source']
        target=u.load_asset(m['target'],follow_redirectors=False)
        assert target and path(u.load_asset(m['source'],follow_redirectors=True))==path(target), '重定向目标身份不符'
        refs=sorted(set(str(x) for x in (REG.get_referencers(m['source'],OPTIONS) or [])))
        if refs:retained.append({'source':m['source'],'refs':refs});continue
        assert u.EditorAssetLibrary.delete_loaded_asset(obj), '无引用重定向清理失败'
        assert u.load_asset(m['target'],follow_redirectors=False)==target, '目标必须保持原对象'
        removed.append(m['source'])
    write('RedirectorCleanup.'+stage+'.json',{'pid':os.getpid(),'removed':removed,'retained':retained})
    print('REDIRECTOR_CLEANUP',len(removed),'removed',len(retained),'retained')
    return retained


def ownership(stage):
    """原生保存包图门禁，包含重定向；旧路径存在不能被当成整改完成。"""
    context()
    edges=[]
    for mount in ('DBAFoliagePack_Core','DBAPresentationPack_Core'):
        for a in REG.get_assets_by_path('/'+mount,recursive=True,include_only_on_disk_assets=True):
            for d in REG.get_dependencies(a.package_name,OPTIONS) or []:edges.append([str(a.package_name),str(d)])
    issues=contract.ownership_issues(DOC,edges)
    write('Ownership.'+stage+'.json',{'pid':os.getpid(),'passed':not issues,'issues':issues})
    print('NATIVE_OWNERSHIP',stage,'issues',len(issues))
    assert not issues, '原生归属门禁未通过：'+str(issues[:2])


def scene_snapshot(stage):
    """同一正式村庄固定镜头及实例变换指纹；不保存或调整世界内容。"""
    context()
    editor=u.get_editor_subsystem(u.LevelEditorSubsystem)
    editor.load_level('/DBAWorldPack_Village/Maps/L_Village_Start')
    actors=[]
    for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
        components=[]
        for component in actor.get_components_by_class(u.InstancedStaticMeshComponent):
            transforms=hashlib.sha256()
            for i in range(component.get_instance_count()):
                transforms.update(component.get_instance_transform(i,False).export_text().encode())
            components.append({'name':component.get_name(),'count':component.get_instance_count(),
                'transformsSHA256':transforms.hexdigest(),'mesh':path(component.get_editor_property('static_mesh'))})
        actors.append({'name':actor.get_name(),'class':actor.get_class().get_path_name(),
            'transform':actor.get_actor_transform().export_text(),'instances':sorted(components,key=lambda x:x['name'])})
    result=normalized({'actors':sorted(actors,key=lambda x:x['name'])})
    write('VillageScene.'+stage+'.json',result)
    if stage!='Before':
        expected=json.loads((OUT/'VillageScene.Before.json').read_text(encoding='utf-8'))
        assert result==expected,'村庄Actor/实例布局发生变化，停止交付'
    u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',False)
    key=editor.get_active_viewport_config_key();editor.editor_set_viewport_realtime(True,key);editor.editor_set_game_view(True)
    editor.set_level_viewport_camera_info(u.Vector(-6500,-500,1700),u.Rotator(pitch=-24,yaw=50,roll=0),key)
    task=u.AutomationLibrary.take_high_res_screenshot(1280,720,str(OUT/('Village.'+stage+'.png')))
    print('VILLAGE_LAYOUT_SNAPSHOT',stage,len(actors),'actors')
    return task
