"""Monolith原生酉鸡64/48审核页、保存重载、导入阻断审计与真实源像素导出。

仅处理第三层酉鸡UI资源，须先执行ImportRoosterV6SkillIconsUE58的import/bind。
复用既有审核布局、原稿颜色审计与引擎导出，不写地图或Gameplay权威资产。
"""
from pathlib import Path
import hashlib
import json
import runpy
import unreal

ROOT = Path(__file__).resolve().parents[3]
PACK = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rooster'
SOURCE = PACK / 'SourceArt/UI/SkillIcons/V6'
OUT = ROOT / 'Game/Saved/Validation/RoosterV6IconImport'

def require(condition, message):
    """失败立即停止，保留本轮证据而不伪造成功。"""
    if not condition:
        raise RuntimeError(message)

def main():
    """制作、审计和导出均在当前正式UE编辑器内执行。"""
    require(Path(unreal.Paths.get_project_file_path()).resolve() == ROOT/'Game/DivineBeastsArena.uproject', '不是正式工程')
    require(unreal.SystemLibrary.get_engine_version().startswith('5.8.0'), '引擎不匹配')
    plan = json.loads((ROOT/'Docs/Implementation/GamePlatformAbilitySystem/RoosterV6SkillIconImportManifest_20261019.json').read_text(encoding='utf-8-sig'))
    lib = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    records, textures = [], []
    for row in plan['entries']:
        for size, field, subdir in ((48, 'runtime48', 'Runtime48'), (64, 'review64', 'Review64')):
            source = SOURCE / subdir / Path(row[field]).name
            require(source.is_file(), '缺少原尺寸原稿')
            target = row['texturePath'] if size == 48 else '/DBAHeroPack_Rooster/UI/Review/V6/Textures/' + source.stem
            if not lib.does_asset_exist(target):
                folder, name = target.rsplit('/', 1)
                task = unreal.AssetImportTask()
                for key, value in {'filename':str(source), 'destination_path':folder, 'destination_name':name,
                                   'automated':True, 'replace_existing':False, 'save':True}.items():
                    task.set_editor_property(key, value)
                tools.import_asset_tasks([task])
            texture = lib.load_asset(target)
            require(isinstance(texture, unreal.Texture2D), '缺少原生Texture2D：' + target)
            for key, value in {'srgb':True, 'compression_settings':unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                               'mip_gen_settings':unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                               'lod_group':unreal.TextureGroup.TEXTUREGROUP_UI,
                               'address_x':unreal.TextureAddress.TA_CLAMP, 'address_y':unreal.TextureAddress.TA_CLAMP,
                               'use_legacy_gamma':False}.items():
                texture.set_editor_property(key, value)
            require(lib.save_loaded_asset(texture, only_if_is_dirty=False), '纹理保存失败')
            textures.append(texture)
            records.append({'Destination':target, 'Source':source.relative_to(ROOT).as_posix(),
                            'SHA256':hashlib.sha256(source.read_bytes()).hexdigest(), 'Role':'Color', 'size':size, 'order':row['order']})
    review_path = '/DBAHeroPack_Rooster/UI/Review/WBP_DBA_Rooster_SkillIconReviewV6'
    widget = lib.load_asset(review_path) if lib.does_asset_exist(review_path) else lib.duplicate_asset(
        '/DBAHeroPack_Ox/UI/Review/WBP_DBA_Ox_SkillIconReviewV6', review_path)
    require(widget is not None, '原生审核布局复制失败')
    prefix = widget.get_path_name() + ':WidgetTree.'
    nodes = {x.get_name():x for x in unreal.ObjectIterator(unreal.Widget) if x.get_path_name().startswith(prefix)}
    white = unreal.LinearColor(1, 1, 1, 1)
    widget.modify()
    for record in records:
        node = nodes['Icon%d_%d' % (record['size'], record['order'])]
        node.set_brush_from_texture(lib.load_asset(record['Destination']), False)
        node.set_color_and_opacity(white)
        node.set_brush_tint_color(unreal.SlateColor(specified_color=white))
        node.get_editor_property('slot').set_size(unreal.Vector2D(record['size'], record['size']))
    nodes['ReviewHeading'].set_text(unreal.Text('Hero.Zodiac.Rooster  仅美术预览、不可施放'))
    nodes['ReviewSize64'].set_text(unreal.Text('64×64审核纹理（原生尺寸）'))
    nodes['ReviewSize48'].set_text(unreal.Text('48×48运行纹理（原生尺寸）'))
    for row in plan['entries']:
        nodes['SkillName%d' % row['order']].set_text(unreal.Text(row['displayNameZh']))
    unreal.BlueprintEditorLibrary.compile_blueprint(widget)
    require(lib.save_loaded_asset(widget, only_if_is_dirty=False), '原生审核页保存失败')
    profile = lib.load_asset(plan['profilePath'])
    packages = [t.get_outermost() for t in textures] + [widget.get_outermost(), profile.get_outermost()]
    ok, error = unreal.EditorLoadingAndSavingUtils.reload_packages(packages, unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    require(ok, '独立重载失败：' + str(error))
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = {'UIRoots':['/DBAHeroPack_Rooster/UI/'], 'Files':records}
    (OUT/'ImportColorManifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    runpy.run_path(str(ROOT/'Tests/Assets/AuditUIColorImportedAssetsMonolith.py'), run_name='__main__', init_globals={
        'IMPORT_MANIFEST':manifest, 'AUDIT_OUTPUT':OUT/'UIColorImportBlockingAudit.json'})
    for record in records:
        target = OUT/'NativeSourceExports'/(Path(record['Destination']).name + '.png')
        target.parent.mkdir(parents=True, exist_ok=True)
        task = unreal.AssetExportTask()
        for key, value in {'object':lib.load_asset(record['Destination']), 'exporter':unreal.TextureExporterPNG(),
                           'filename':str(target), 'automated':True, 'prompt':False}.items():
            task.set_editor_property(key, value)
        require(unreal.Exporter.run_asset_export_task(task), '真实纹理源像素导出失败')
        record['NativeSourceExport'] = target.relative_to(ROOT).as_posix()
    (OUT/'NativeTextureReload.json').write_text(json.dumps({'nativeTextureCount':16, 'reviewWidget':review_path,
        'savedAndReloaded':True, 'textures':records, 'pixelComparisonPending':True, 'manualAccepted':False}, ensure_ascii=False, indent=2), encoding='utf-8')
    print('SSLM_ROOSTER_V6_REVIEW_16_TEXTURES_SAVED_RELOADED_AUDITED')

if __name__ == '__main__':
    main()
