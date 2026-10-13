# SSLM多区域自然飘落与漂浮粒子资产库：交付与验证记录

版本：2026-10-11。工作区：DivineBeastsWorkspace（SSLM工作空间），UE5.8。

## 一、资产所有权与三层架构

- GamePlatform（平台基础层）：`GamePlatformVFX（游戏平台视觉特效插件）`拥有统一的Niagara执行、限额、组件生命周期和平台通用母材质及漂浮模板；`GamePlatformWeather（游戏平台天气插件）`继续拥有雨、雪及风场事实；`GamePlatformPCG（游戏平台程序化生成）`可提供地表/树冠区域锚点，但不直接播放装饰粒子。
- MobaCommon（MOBA通用层）：纯装饰环境不创建重复实现，竞技地图按平台表现接口复用。
- DivineBeasts（SSLM项目层）：`DBAPresentationPack_Core（公共视觉内容包）`拥有专用纹理、材质实例、Niagara外观变体、Definition（特效定义）与区域Profile（数据资产）；`DBAClient（项目客户端代码插件）`只做目录、区域选择和调用协调，不重新建立Niagara播放器。服务端不同步装饰粒子。

跨层依赖只允许项目 → 平台；平台材质的默认纹理属于引擎或平台，不硬引用SSLM专属Texture2D。

## 二、分类、资产与完成范围

目录 `SourceArt/AmbientNature/AmbientNatureStyleCatalog.json（39种风格清单）` 包含：
- 花瓣类：梨花、樱花、梅花、杏花、玉兰、荷花、桂花、玫瑰、通用粉/白/红花瓣、彼岸花；加已有桃花。
- 树叶类：梧桐、柳叶、杨树、橡树、白桦、通用黄叶/红叶/枯叶、松针、棕榈；加已有竹叶、枫叶、银杏叶。
- 其他飘落/漂浮类：柳絮、蒲公英、羽毛、纸片、碎纸、灰烬、火星、沙尘、尘埃、孢子、萤火虫。
- 现有天气代理：雪花、雨滴/细雨。两项直接引用平台天气Definition，禁止复制雨雪执行机制。

本批实际新增33类项目风格，经UE5.8源导入共生成：
- 66张1024×1024 RGBA源颜色/法线PNG与对应真实Texture2D（项目专用纹理）。
- 33个MaterialInstance（材质实例），继承平台双面植被、柔和半透明或发光母材质。
- 33套项目Niagara外观系统变体。花瓣/树叶/纸屑复用已有FallingFoliage运动模板；柳絮、羽毛、蒲公英、灰烬、孢子、萤火虫等复用`NS_GP_AmbientFloat_Base（平台漂浮模板）`，该模板为雪花缓慢漂移机制的第一期复用版本。
- 33份`UGamePlatformVFXWorldDefinition（平台世界环境特效定义）`，维持项目独立`presentation.dba.environment.ambient.*@1`逻辑身份。
- 平台新增两份通用材质母版（Soft/Glow）及一个Float基础Niagara；雨雪没有复制。

**验收边界**：上述是真实存盘的引擎资源；所有新系统仍需在D3D12目标渲染和目标地图进一步检查粒子可见性、发光材质亮度、无穿地、动态高度、性能降级。漂浮模板不等于火星上升、萤火虫趋光、风吹纸屑等专属运动全部已实现；不得报Production Ready。

## 三、区域与资源数据化

`SourceArt/AmbientNature/AmbientNatureAreaProfiles.json（21组候选区域预设）`规定 `AreaKey（区域身份）` → `StyleId（视觉风格）` 与各风格权重、环境风量及高度。区域候选包括前端工作室、桃林、梨园、竹林、荷塘、桂花院、银杏院、红枫坡、梧桐大道、混交林、柳岸、松林、热带林、春花园、奇幻花田、蒲公英原野、节日广场、遗迹沙尘、篝火余烬、雨区、雪区。

对应项目层C++类型 `UDivineBeastsAmbientRegionProfile（区域环境视觉预设数据资产）` 及 `FDivineBeastsAmbientStyleWeight（风格权重结构）` 已新增。权重结构同时包含 `StyleId（风格身份）` 与 `DefinitionId（项目VFX主资产逻辑身份）`，通过 `SelectStyle（按权重选择风格）` / `SelectDefinitionId（按相同权重取得特效定义ID）` 保证区域配置可交给 GamePlatformData（平台数据服务）按既有主资产合同预载，而非另外硬编码39种StyleId。该类型只承载数据，不直接运行Niagara或更改服务器天气。对应 `DivineBeastsAmbientRegionProfileTests.cpp（区域预设权重及Definition选择自动化测试）` 已提交源码，必须在Editor模块完成真实编译后执行，不得提前标记通过。

`Tools/Unreal/VFX/FallingFoliage/AuthorAmbientRegionProfilesUE58.py（UE5.8区域数据资产作者脚本）` 按JSON生成21个真实DataAsset，但必须先通过Editor目标编译并刷新反射类型。仅有候选区域配置或未连接的区域DataAsset不等于已经在世界地图激活；PCG与真实关卡仍须显式把锚点/区域身份传递给现有表现服务。

## 四、正式资源目录

```text
Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/AmbientNature/
  Materials/                 # 平台柔和/发光母材质
  Niagara/                   # 平台通用漂浮模板

Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/
  SourceArt/AmbientNature/   # 项目源图/哈希/风格目录/区域候选/参考来源标记
    Textures/                # 33类真实RGBA颜色与法线PNG
  Content/VFX/Environment/AmbientNature/
    Textures/                # 66张实际UE纹理
    Materials/               # 33份项目材质实例
    Niagara/                 # 33个外观变体系统
    Definitions/             # 33份项目Definition
    Regions/                 # 21份区域Profile（需真实Editor加载及保存后确认）
```

当前已存在桃花、枫叶、竹叶、银杏叶仍保留在`FallingFoliage/（已有资源身份目录）`，不进行易破坏引用的迁移；新增类使用`AmbientNature/（环境效果扩展目录）`。

## 五、AI素材原图使用边界

本对话生成的梨花、樱花、银杏、梧桐/红枫五套精细RGBA图集已在ChatGPT工作容器中整理为可下载源包，另提取164个独立精灵与哈希清单。`import_conversation_files_to_project（会话附件导入工程）` 接口拒绝该模型生成文件，提示可信宿主来源缺失。因此**这些精细图片目前没有被直接上传到Windows项目工程**。项目内程序生成的33种纹理属于有区别的可编辑初始资源，不得冒充原图。这是正式美术替换与人工确认之前的剩余事项。

## 六、自动验证与待办

实际证据在`Saved/Validation/FallingFoliage/ExpandedLibrary/（不提交的本地UE审查日志）`：
- `GeneratedTextureManifest.json（源纹理SHA-256清单）`：66张源PNG逐一校验成功。
- `UEImportReport.json（真实资产导入记录）`：33种Texture/Material/Niagara/Definition真实写盘成功。
- `UEAssetIntegrationAudit.json（资源依赖检测）`：校验项目Niagara内部Sprite材质绑定、定义与系统、天气代理、21区风格引用。
- `AmbientRegionClientBuild.log（项目区域C++客户端编译日志）`：Client编译已通过，但Editor模块链接、区域DA创建及地图激活应单独验收。
- `Game/Config/DefaultGame.ini（统一主资产扫描配置）` 已加入新增Definition目录；`FrontEndClient/DefaultGame.ini`与`VillageDevelopmentClient/DefaultGame.ini`已加入客户端VFX Cook目录。DedicatedServer既有Pak资源排除规则保持原状。

在地图/PCG激活前必须完成真实UE5.8数据资产编译及保存、角色选择创建和World/Village区域的视觉巡检、D3D12/Mobile性能及Cook/Stage和服务器资源剥离确认。任何失败记录及编辑器并发占用均必须保留，不伪造通过。

## 七、本轮验收和阻断状态（截至2026-10-11 16:33）

- 新增资源实际写盘：33套项目外观的Niagara、Definition和材质实例，66张Texture2D；平台新增2张通用母材质、1套漂浮模板；66张源PNG通过SHA-256。原有4种飘落和雨雪2种没有重复实现。
- 引擎Asset审查初次出现2项天气类型误判，修正后又发现4项原有落叶Definition类型误判，已修正脚本。最后一次复核卡在共享UnrealBuildTool互斥锁，直至超时；不能声称该项已经0错误通过。
- 最新静态资源门禁实际执行276项通过、0项失败，但不包括真实Niagara播放或客户端Cook。
- 区域类型早期Client定向编译成功，但最新增加的DefinitionId字段、SelectDefinitionId、区域选择自动化测试尚未重新编译验证；Editor构建受到其他会话的UBT互斥锁阻断。本会话只停止了自己排队的任务，未中断用户编辑器或其他会话。
- 21组AreaProfile目前是候选JSON及真实DataAsset生成脚本，实际Regions目录尚无真实区域.uasset；它们尚未绑定地图/PCG，也不能声称不同区域已经正常显示。
- 当前ChatGPT生成的五张精品原图与164个分离精灵已导出ZIP，但连接器可信文件导入接口拒绝上传远端；项目内新纹理仍是另外制作的程序纹理。原图进入项目需要人工可信文件传输，导入脚本已准备SHA校验及备份步骤。
- 剩余：Editor实际编译、21区域DataAsset落盘/回读、所有主资产注册、实际区域Actor/PCG激活、Niagara色彩/高空落叶/贴地风吹修正、客户端Cook/Stage和Dedicated Server纯视觉剥离、GPU性能及人工签收。不能拿静态验收替代运行验收。

