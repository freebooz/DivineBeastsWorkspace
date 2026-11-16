"""由Monolith执行：把现有英雄八槽图接到真实授权技能ID，不改变技能机制或美术审批。

默认inspect，只加载现有Profile、AbilitySet和Texture2D并保存逐槽证据；apply只修改
现有Profile条目的Icon，先全量预检并备份原包，verify要求所有真实授权槽映射一致。
八槽图来自已有Profile的显式ArtPreviewAbilityIds；只有V1的英雄按旧作者清单的
HeroDefinitionId+InputTag+IconAssetPath接前四槽，其余未制作槽保持临时图并明确报告。
不根据文件名、条目顺序或中文名猜测，不把现存预审图或开发清单升格为最终审批。
"""
from pathlib import Path
import json
import shutil
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(globals().get('HOTBAR_ICON_OUTPUT', ROOT / 'Saved/Validation/HotbarHeroIcons20261021'))
MODE = globals().get('HOTBAR_ICON_MODE', 'inspect')
HEROES = ('Rat', 'Ox', 'Tiger', 'Rabbit', 'Dragon', 'Snake', 'Horse', 'Goat', 'Monkey', 'Rooster', 'Dog', 'Boar')
INPUTS = ('Primary', 'Slot1', 'Slot2', 'Slot4', 'Slot3', 'Slot5', 'Slot6', 'Slot7')


def require(value, message):
    if not value:
        raise RuntimeError(message)


def logical_id(value):
    return (str(value.get_editor_property('Namespace')) + '.' +
            str(value.get_editor_property('Name')) + '@' + str(value.get_editor_property('LogicalVersion'))).lower()


def entry_snapshot(entry):
    icon = entry.get_editor_property('Icon')
    return {'id': str(entry.get_editor_property('AbilityId')),
            'name': str(entry.get_editor_property('DisplayName')),
            'description': str(entry.get_editor_property('Description')),
            'icon': icon.get_path_name() if icon else ''}


def inspect_hero(hero):
    """每个预期图都必须是已存在且属于当前英雄的Texture2D，缺一项即停止整个迁移。"""
    lib = unreal.EditorAssetLibrary
    mount = '/DBAHeroPack_' + hero
    profile_path = mount + '/UI/Abilities/DA_DBA_' + hero + '_PlaceholderUIProfile'
    profile = lib.load_asset(profile_path)
    require(isinstance(profile, unreal.DivineBeastsAbilityUIProfile), hero + ' 缺少真实Profile')
    require(str(profile.get_editor_property('HeroDefinitionId')) == 'Hero.Zodiac.' + hero, hero + ' Profile身份错误')
    matches = []
    for data in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(mount, recursive=True):
        if str(data.asset_class_path.asset_name) == 'DivineBeastsAbilityUIProfile':
            candidate = lib.load_asset(str(data.package_name))
            if str(candidate.get_editor_property('HeroDefinitionId')) == 'Hero.Zodiac.' + hero:
                matches.append(str(data.package_name))
    require(matches == [profile_path], hero + ' Profile主资产不唯一：' + str(matches))
    entries = list(profile.get_editor_property('Entries'))
    snapshots = [entry_snapshot(entry) for entry in entries]
    by_id = {row['id']: entry for row, entry in zip(snapshots, entries)}
    require(len(by_id) == len(entries), hero + ' Profile条目身份重复')
    art_ids = [str(value) for value in profile.get_editor_property('ArtPreviewAbilityIds')]
    require(not art_ids or (len(art_ids) == 8 and len(set(art_ids)) == 8), hero + ' 显式八槽图映射损坏')
    targets = {}
    if art_ids:
        for index, suffix in enumerate(INPUTS):
            require(art_ids[index] in by_id, hero + ' 显式图标条目缺失 ' + art_ids[index])
            targets[suffix] = (by_id[art_ids[index]].get_editor_property('Icon'), art_ids[index],
                               'ExistingProfileExplicitEightSlotArt')
    else:
        # 历史清单仅作为用户本次授权的既有图来源，不提供任何GAS授权或数值。
        catalog = json.loads((ROOT / 'Docs/Implementation/ZodiacAbilityDevelopmentDraft_20261009.json').read_text(encoding='utf-8-sig'))
        manifest = json.loads((ROOT / ('Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_' + hero +
                                       '/Docs/MonolithGenerationManifest.json')).read_text(encoding='utf-8-sig'))
        for item in catalog['Entries']:
            if item['HeroDefinitionId'] != 'Hero.Zodiac.' + hero or not item['InputTag']:
                continue
            suffix = item['InputTag'].removeprefix('Platform.Ability.Input.DivineBeasts.')
            require(suffix in ('Primary', 'Slot1', 'Slot2', 'Slot4') and suffix not in targets, hero + ' 旧作者槽映射冲突')
            require(any(asset['AssetPath'] == item['IconAssetPath'] and asset['Slot'] == item['Slot']
                        for asset in manifest['Assets']), hero + ' 旧原图未被Monolith导入清单证明')
            targets[suffix] = (lib.load_asset(item['IconAssetPath']), '',
                               'LegacyCatalogExplicitInputTagAndNativeManifest')
        require(len(targets) == 4, hero + ' 旧作者清单不是四个主动槽')
    ability_set = lib.load_asset('/DBAGameplay/Abilities/Placeholders/DA_DBA_' + hero + '_PlaceholderAbilitySet')
    require(isinstance(ability_set, unreal.GamePlatformAbilitySetDefinition), hero + ' 缺少真实授权集')
    grants = list(ability_set.get_editor_property('Abilities'))
    rows = []
    for index, suffix in enumerate(INPUTS):
        input_tag = 'Platform.Ability.Input.DivineBeasts.' + suffix
        matches = [grant for grant in grants if str(grant.get_editor_property('InputTag').get_editor_property('TagName')) == input_tag]
        require(len(matches) == 1, hero + ' 没有唯一授权槽 ' + input_tag)
        granted_id = logical_id(matches[0].get_editor_property('AbilityId'))
        require(granted_id in by_id, hero + ' 授权表现条目缺失 ' + granted_id)
        current = by_id[granted_id].get_editor_property('Icon')
        require(isinstance(current, unreal.Texture2D), hero + ' 当前临时图不是已存在Texture2D ' + granted_id)
        target = targets.get(suffix)
        source, art_id, provenance = target if target else (None, '', 'RetainedTemporary_NoAuthoredSlotIcon')
        if target:
            require(isinstance(source, unreal.Texture2D), hero + ' 角色槽图不是已存在Texture2D ' + suffix)
            require(source.get_path_name().startswith(mount + '/UI/Abilities/'), hero + ' 角色槽图归属错误 ' + source.get_path_name())
        rows.append({'key': suffix, 'inputTag': input_tag, 'grantedAbilityId': granted_id,
                     'artAbilityId': art_id, 'expectedTexture': source.get_path_name() if source else '',
                     'actualTexture': current.get_path_name() if current else '',
                     'matches': current == source if source else True, 'canBind': bool(target),
                     'provenance': provenance, 'textureClass': (source or current).get_class().get_name(),
                     'dimensions': str(lib.find_asset_data((source or current).get_path_name()).get_tag_value('Dimensions'))})
    return {'hero': hero, 'profile': profile, 'entries': entries, 'before': snapshots,
            'artIds': art_ids, 'rows': rows}


def main():
    require(Path(unreal.Paths.get_project_file_path()).resolve() == ROOT / 'Game/DivineBeastsArena.uproject', '非正式工程')
    require(unreal.SystemLibrary.get_engine_version().startswith('5.8.0'), '非锁定UE5.8.0')
    require(MODE in ('inspect', 'apply', 'verify'), '未知执行模式')
    OUT.mkdir(parents=True, exist_ok=True)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(['/DBAHeroPack_' + hero + '/UI/Abilities' for hero in HEROES], force_rescan=True)
    checks = [inspect_hero(hero) for hero in HEROES]
    if MODE == 'apply':
        require(not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '存在未保存资产，先保留现场')
        baseline = {check['hero']: check['before'] for check in checks}
        path = OUT / 'OriginalProfileEntries.json'
        require(not path.exists(), '已有迁移基线，拒绝覆盖；请先inspect/verify现状')
        path.write_text(json.dumps(baseline, ensure_ascii=False, indent=2), encoding='utf-8')
        # 所有目标先读取和备份，任何缺图或身份冲突均在首个资产修改前失败。
        for check in checks:
            hero = check['hero']
            disk = ROOT / ('Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_' + hero +
                           '/Content/UI/Abilities/DA_DBA_' + hero + '_PlaceholderUIProfile.uasset')
            require(disk.is_file(), 'Profile磁盘包不存在 ' + str(disk))
            backup = OUT / 'Backup' / disk.relative_to(ROOT)
            backup.parent.mkdir(parents=True, exist_ok=True)
            require(not backup.exists(), '已有原包备份，拒绝覆盖 ' + hero)
            shutil.copy2(disk, backup)
        for check in checks:
            if not any(row['canBind'] and not row['matches'] for row in check['rows']):
                continue
            by_id = {str(entry.get_editor_property('AbilityId')): entry for entry in check['entries']}
            for row in check['rows']:
                if row['canBind']:
                    by_id[row['grantedAbilityId']].set_editor_property('Icon', unreal.load_asset(row['expectedTexture']))
            check['profile'].set_editor_property('Entries', [by_id[row['id']] for row in check['before']])
            require(unreal.EditorAssetLibrary.save_loaded_asset(check['profile'], only_if_is_dirty=False), 'Profile保存失败 ' + check['hero'])
        checks = [inspect_hero(hero) for hero in HEROES]
    baseline_path = OUT / 'OriginalProfileEntries.json'
    if baseline_path.exists():
        baseline = json.loads(baseline_path.read_text(encoding='utf-8'))
        for check in checks:
            before = baseline[check['hero']]
            current = check['before']
            changed_ids = {row['grantedAbilityId'] for row in check['rows'] if row['canBind']}
            require(len(before) == len(current), 'Profile条目数量被改变')
            for old, new in zip(before, current):
                require(all(old[key] == new[key] for key in ('id', 'name', 'description')), '技能身份或文字被改变')
                if old['id'] not in changed_ids:
                    require(old['icon'] == new['icon'], '非授权展示别名图标被改变')
    report = {'mode': MODE, 'heroes': [{'hero': check['hero'], 'profile': check['profile'].get_path_name(),
              'artIds': check['artIds'], 'slots': check['rows']} for check in checks],
              'checkedSlots': sum(len(check['rows']) for check in checks),
              'mappedSlots': sum(row['canBind'] for check in checks for row in check['rows']),
              'retainedTemporarySlots': sum(not row['canBind'] for check in checks for row in check['rows']),
              'mismatches': sum(not row['matches'] for check in checks for row in check['rows']),
              'gameplayChanged': False, 'approvalChanged': False, 'humanAccepted': False, 'runtimeHUDVerified': False}
    (OUT / ('Profiles_' + MODE + '.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'mode': MODE, 'checkedSlots': report['checkedSlots'], 'mappedSlots': report['mappedSlots'],
                      'retainedTemporarySlots': report['retainedTemporarySlots'], 'mismatches': report['mismatches'],
                      'report': str(OUT / ('Profiles_' + MODE + '.json'))}))
    if MODE in ('apply', 'verify'):
        require(report['mismatches'] == 0, '真实授权技能图尚有错误映射')
    return report


if __name__ == '__main__':
    main()
