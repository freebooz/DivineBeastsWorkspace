"""从唯一插件清单生成HUB只读视觉数据；不创建或伪造UE资产。"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).with_name('PluginHubCards.json')
manifest = json.loads((ROOT/'Docs/Implementation/PluginDemos/PluginDemoManifest.json').read_text(encoding='utf-8-sig'))
levels = json.loads((ROOT/'Docs/Design/PluginDevelopment20261114/LevelCatalog.json').read_text(encoding='utf-8-sig'))['Levels']
visuals = {item['Plugin']: item for item in levels}
descriptions = {
 'GamePlatformPCG':'生成森林与植被，控制密度、坡度和区域避让。',
 'GamePlatformSurface':'预览材质层次，调节颜色、粗糙度与表面细节。',
 'GamePlatformVFX':'查看法术与环境特效，调整形态、色彩和表现。',
 'GamePlatformUI':'预览通用控件与主题，检查布局和交互状态。',
 'GamePlatformWater':'调节湖泊与河流，预览水面、岸线与水下表现。',
 'GamePlatformWeather':'预览晴雨雪雾，调整光照、强度和天气过渡。',
 'GamePlatformSFX':'试听环境与技能音效，检查音量、衰减和空间声场。',
 'GamePlatformWorld':'检查世界上下文、区域加载与进入退出生命周期。',
 'GamePlatformAI':'观察行为决策、目标选择与移动，验证中止和恢复。',
 'GamePlatformAnimation':'预览角色动作、挂点和过渡，核对动画播放状态。',
 'GamePlatformCharacter':'检查角色生成、能力组件与状态切换。',
 'GamePlatformCombat':'检查权威伤害、治疗与投射物，核对战斗结果。',
 'GamePlatformAbilitySystem':'检查施法资格、消耗与冷却，验证取消和失败。',
 'GamePlatformGameplay':'检查玩法状态、规则组合与生命周期。',
 'GamePlatformCore':'查看稳定身份、版本与基础契约的诊断信息。',
 'GamePlatformData':'检查资源发现、加载与租约，验证失败和释放。',
 'GamePlatformApplicationFlow':'查看应用主流程，验证进入、取消和恢复路径。',
 'GamePlatformInput':'检查键鼠与手柄输入、焦点移动和设备切换。',
 'GamePlatformLoading':'查看加载反馈与转场，检查进度、取消和失败。',
 'GamePlatformLocalization':'检查中文与多语言文本、格式和语言切换。',
 'GamePlatformSave':'检查存档读取、写入与恢复，使用隔离测试数据。',
 'GamePlatformSettings':'调节声音与画面偏好，检查应用和恢复默认值。',
 'GamePlatformDebug':'查看开发诊断与状态，定位运行异常。',
 'GamePlatformDeveloperTools':'核查资源归属、引用和架构规则，查看校验报告。',
 'GamePlatformTelemetry':'查看运行事件与诊断数据，检查采集生命周期。',
 'GamePlatformLobby':'检查常驻世界大厅体验和入口，不创建独立服务器角色。',
 'GamePlatformVillage':'检查新手教学、训练流程与体验切换。',
 'GamePlatformQuest':'检查任务目标、进度通知与完成反馈。',
 'GamePlatformOnline':'检查在线服务连接、请求失败与重试状态。',
 'GamePlatformServer':'查看专服注册、就绪与准入，操作前核验执行资格。',
 'GamePlatformSession':'检查会话建立、断线恢复与世界迁移。',
 'GamePlatformCommerceUI':'预览交易控件与操作反馈，验证真实资格显示。',
 'GamePlatformEntitlement':'检查权益资格与状态通知，验证拒绝和失效。',
 'GamePlatformEquipment':'检查装备槽位、挂点和外观同步。',
 'GamePlatformInventory':'检查背包、物品数量与变化通知。',
 'GamePlatformLiveOps':'检查活动配置、开放条件与运行状态。',
 'GamePlatformProgression':'检查角色成长、进度变化与状态反馈。',
 'GamePlatformCamera':'调节镜头位置与视角，检查跟随和过渡。',
 'GamePlatformPresentation':'核对表现语义、内容映射与失败回退。',
 'GamePlatformInteraction':'检查交互对象、距离与资格反馈。',
 'GamePlatformNavigation':'查看寻路与移动状态，检查障碍、取消和恢复。',
 'GamePlatformArena':'检查通用竞技模式、队伍配置与比赛流程。',
 'MobaPresentation':'检查竞技事实到表现语义的转换与去重。',
 'DBAArena':'检查SSLM竞技装配、模式配置与比赛入口。',
 'DBAClient':'检查SSLM客户端界面、输入与表现装配。',
 'DBAGameplay':'检查项目技能定义与战斗映射，保留服务器裁决。',
 'DBAServer':'查看项目专服装配、角色配置与启动资格。',
 'DBAWorlds':'检查项目世界定义、出生配置与地图装配。',
 'DBAFoliagePack_Core':'浏览树木、灌木与草类，检查材质和植被配置。',
 'DBAFrontEndPack':'浏览登录与选角内容，检查镜头和界面组合。',
 'DBAContentPack_Common':'浏览通用角色模型、动画和共享内容。',
 'DBAPresentationPack_Core':'浏览公共法术特效与表现定义，检查资源引用。',
 'DBASFXPack_Core':'试听公共环境和战斗音频，检查资源与声音配置。',
 'DBAUIPack_Core':'浏览公共控件、玉石主题、字体和界面纹理。',
 'DBAWorldPack_MainArena':'浏览竞技场地图与内容，检查场景资源引用。',
 'DBAWorldPack_Village':'浏览新手村场景与内容，保留现有手工设计。',
 'BlueprintAutoLayout':'整理蓝图节点布局，检查排列与连线可读性。',
 'GlooPrint':'整理蓝图排版和注释，保持现有执行逻辑。',
}
cards = []
for plugin in manifest['plugins']:
    identity=plugin['plugin_id']; visual=visuals[identity]
    description=descriptions.get(identity)
    if identity.startswith('DBAHeroPack_'):
        description='浏览'+visual['Title'].replace('资源','')+'的模型、技能图标与专属表现资源。'
    if not description:
        raise ValueError('Missing description: '+identity)
    name = identity.replace('_', '')
    cards.append({'PluginId':identity,'Title':visual['Title'],'Description':description,
        'Image':'Tools/Unreal/PluginDemos/Art/'+name+'.png',
        'Texture':'/DBAUIPack_Core/Development/PluginDemos/Textures/Plugins/TPluginHub'+name})
assert len(cards)==len({c['PluginId'] for c in cards})==len(manifest['plugins'])
OUT.write_text(json.dumps({'Schema':1,'Purpose':'PresentationOnly','IdentitySource':'Docs/Implementation/PluginDemos/PluginDemoManifest.json','Columns':3,'Pagination':False,'CardWidth':580,'CardHeight':360,'ThumbnailAspect':'16:9','Artwork':'用途插画，不代表运行或审核通过','Cards':cards},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Presentation data:',len(cards),'cards; identities match current manifest.')
