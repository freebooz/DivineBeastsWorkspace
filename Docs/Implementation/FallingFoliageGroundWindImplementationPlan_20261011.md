# 神兽联盟自然飘落与落地风吹特效实施计划（UE5.8）

更新：2026-10-11。工程：DivineBeastsWorkspace（神兽联盟工作空间）；实施工具：HWebCodex（代码工作区连接器）。本计划为增量执行清单，不替代真实编辑器和设备验证。

## 1. 目标与约束

在同一套GamePlatformVFX（游戏平台视觉特效）运动逻辑上复用桃花、枫叶、竹叶、银杏四种外观，具备空中飘落→接触地面→静止→贴地滑动/翻滚→阵风扬起→再次飘落→回收的连续状态。角色选择与角色创建共用L_DBA_CharacterStudio（角色预览关卡）中的桃花效果，不重建地图与UI，不同步纯装饰粒子。

遵守AGENTS.md（工程规则）：三层单向依赖，通用代码留GamePlatformVFX；MOBA通用层不增加无职责类型；项目外观留DBAPresentationPack_Core，前端关卡与地面锚点留DBAFrontEndPack；通过GamePlatformPresentation（表现目录）→GamePlatformData（数据租约）→GamePlatformVFX（执行服务）装配。专用服务器不得装入纯表现资源。已有未提交文件不得覆盖；.uasset和.umap必须由真实UE5.8生成，Widget资产仅通过Monolith MCP（编辑器UI工具）修改。

## 2. 分阶段门禁

- P0 基线与并发：核对Git HEAD及现有未提交改动、UE5.8工程身份、编辑器/Monolith连接、资产路径与真实资源类别。锁定修改范围；存在重叠后台任务时先等待，禁止并发覆盖。交付资源清单。
- P1 源纹理：使用已有GenerateFallingFoliageSourceTextures.py（源纹理生成器）核验四类2048×2048颜色/法线PNG，检查透明边缘、SHA-256及美术预览；缺失才增量生成，禁止重做已核准资源。交付8张源纹理和Manifest（清单）。
- P2 材质：检查M_GP_FallingFoliage_Master（平台母材质）双面、Masked透明裁剪、法线、色彩及近景照明；检查四份MI_DBA_Foliage_*（项目材质实例），通过UE编辑器真实保存并回读。不得将项目纹理置于平台层。
- P3 Niagara运动：检查共享NS_GP_FallingFoliage_Base与NM_GP_FoliageAirMotion/NM_GP_FoliageGroundMotion（空中/地面运动模块）。完善接触平面、切向滑动、随机旋转、阵风阈值和二次扬起；在单实例粒子状态中转换，避免每帧CPU追踪或重复播放。四套项目Niagara变体必须复用共享运动核心。设置NET_GP_FallingFoliage_Ambient（环境特效性能类型）。
- P4 定义和目录：核查四份UGamePlatformVFXWorldDefinition（世界特效定义），逻辑ID采用namespace.name@version（命名空间.名称@版本），由GamePlatformData加载，默认参数服从Schema白名单及单请求覆盖上限；注册项目内容包和目录时原子预加载，失败回滚，不把资产存在当播放成功。
- P5 代码与蓝图：在DBAClient（项目客户端插件）复用现有角色预览激活/卸载事件与平台表现接口；角色切换不重建特效、场景卸载先取消请求；在DBAFrontEndPack（前端内容包）中使用BP_DBA_FoliageGroundAnchor（地面锚点蓝图）提供落地视觉参考高度。2026-10-11实际引擎地图已切换为Landscape（地形），无原PreviewFloor（平面地板），当前实现仅使用锚点Z和向上法线进行局部平面近似；不依赖游戏玩法碰撞，也不伪称对复杂地形完成准确贴合。角色舞台和锚点必须属于同一Level（关卡实例）。
- P6 关卡：UE5.8打开L_DBA_CharacterStudio，保留现有角色舞台、曝光与镜头，编辑器中绑定地面锚点及桃花定义；实现角色选择/创建共享环境表现。禁止遮挡面部、按钮、名称输入；关卡保存/重开回读。
- P7 自动化及编译：先只读资源验证→UE编辑器AssetRegistry与DataValidation（资产登记与验证）→Niagara/材质编辑器编译→Editor和Client受影响模块构建→核心用例→Client Cook/Stage（客户端烘焙/分发）和Server纯表现剥离审计。每一阶段记录目标、命令、退出码、日志，编译不等于运行验证。
- P8 人工验证：登录→角色选择→创建→选择切换→退出/重新进入；测试四套风格以及零风、微风、阵风、二次扬起，观察地面穿插、闪烁、堆积、迟到回调、GPU性能和前端布局。人工记录截图、录像、运行环境与未通过事项后方可签收。

## 3. 当前已核查事实与未完成门禁

2026-10-11 HWebCodex读取到四类源颜色/法线纹理共8张，单张2048×2048且可被Windows图像解码；项目实际存在四类材质实例、四份Niagara系统及定义、平台共享Niagara基础系统/空中/地面运动模块、环境性能资产、地面锚点蓝图和角色预览关卡。以上仅为文件级证据，未判定资产引用、Niagara编译或显示正确。

另有编辑器构建Job超时，输出停在约1161/3982动作，不能计入成功。Runner内没有发现Monolith MCP注册；不能在缺少连接时假称已编辑并核验真实Widget。系统仅找到WindowsApps的python占位程序，不能用本机python命令执行源纹理校验脚本；已通过Windows System.Drawing与SHA-256抽样核验源文件的真实存在与尺寸。下一轮需恢复合格Python或通过锁定编辑器脚本执行校验。

## 4. 正式验收

1. 四种外观在同一运动内核替换，不复制播放器；
2. 落地、停留、滑动、翻滚、二次扬起闭环可见，无突变；
3. 选角、创建页面共享同一场景特效且退出时清理；
4. 参数/目录/资源缺失有真实拒绝和回退，不伪造提交成功；
5. 资源、Editor/Client构建、Cook/Stage和人工视觉分别有真实证据；
6. 不越过原有用户未提交改动，不在服务器包含专用客户端资产。

## 5. 执行状态

- **P0：已完成基线检查。** 工作区主分支HEAD为`01369680`，确认当前存在其他任务未提交改动，不触碰其UI、雪地或设置插件源码；Monolith MCP（界面编辑器接口）未出现在HWebCodex可用服务器列表中。
- **P1：完成源纹理文件验证。** 桃花、枫叶、竹叶、银杏叶的颜色/法线PNG共8张，全部为2048×2048，逐张与源Manifest的SHA-256一致。
- **P2—P4：完成引擎内结构审查，不等于GPU运行验收。** UE5.8按真实挂载路径加载25个Niagara/材质/纹理/Definition资产，0个资源加载及目标类型错误；四份材质实例均指向同一平台母材质，4个项目Niagara均引用共享空中和地面运动模块及各自项目材质，Definition逻辑身份与System软引用成立。证据：`Saved/Validation/FallingFoliage/UEAssetInspection.json`（引擎只读审查）。
- **P5—P6：已完成现有代码/关卡修正及引擎回读。** 实际`L_DBA_CharacterStudio`包含Landscape（地形），并非旧文档描述的PreviewFloor；9个真实场景Actor，包含1个`DBA.Foliage.Ground`锚点，当前高度Z=-1厘米。修复`DivineBeastsCharacterPreviewSubsystem.cpp`为同一Level实例过滤并选择最近锚点；无新增Widget/UI修改。证据：`Saved/Validation/FallingFoliage/StudioMapInspection.json`。
- **P7：部分编译与发布门禁通过，正式整包仍未验证。** UE5.8 `DivineBeastsArenaEditor（编辑器目标）`对修改的预览子系统单文件编译成功（14.11秒）；`DivineBeastsArenaClient（客户端目标）`的`DivineBeastsPresentationClient`模块两项C++编译成功（15.73秒）。新增客户端Cook收录目录和IoStore必需资源门禁：静态验证27/27资源存在、3项客户端目录收录、3项服务器排除、PowerShell脚本解析0错误。证据：`Saved/Validation/FallingFoliage/UE58_FoliagePresentationSingleFileBuild.log`及`UE58_FoliageClientModuleBuild.log`。上述模块定向构建不等于完整Client/Editor链接，也不等于Cook/Stage成功。
- **P7 当前阻断和限制：** 编辑器自动化命令匹配`DivineBeasts.Presentation.FallingFoliage.CatalogMapping`得到**0项执行**，因为已加载的`DivineBeastsPresentationRuntime` Editor DLL早于该测试源码，不能记为通过。随后独立UE5.8启动还出现`GamePlatformSettingsRuntime` DLL缺失；这来自同时存在的独立设置插件装配变更，不擅自覆盖其源码。E盘剩余空间约9.5GB，大规模Cook/Stage需在确认磁盘空间和并发工作区稳定后进行。Monolith及最终Niagara GPU编译/性能没有真实通过证据。
- **P8：待人工验证。** 需在客户端登录后分别进入角色选择/创建，确认持续桃花、贴地滑动、二次扬起以及风格替换、退出清理、性能预算，保存设备帧时间/截图和检查结论。

## 6. 后续增量处置与最新验证（2026-10-11）

- 修复缺失的`GamePlatformDefinition（平台主资产定义）`扫描入口：`Game/Config/DefaultGame.ini`已追加`/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Definitions`，使Editor/Client在相同的主资产规则下发现四种逻辑ID。路径注册正确仅表示有资格被扫描，成功加载仍依赖实际Data服务验证。
- 完整资源静态门禁：新增`Tests/Architecture/TestFallingFoliageDelivery.ps1`，适配PowerShell 5.1（UTF-8 BOM编码），Windows远程执行`FALLING_FOLIAGE_STATIC_AUDIT passed=True checks=62 failures=0`；报告`Saved/Validation/FallingFoliage/StaticDeliveryAudit.json`。检查包括8张源PNG哈希、27份关键UE资产、主资产扫描、目录逻辑ID、客户端Cook收录、服务器Pak排除、IoStore检查脚本语法；**不能替代UE编辑器编译或最终资源容器验证**。
- 前端人工检查指引在`Docs/Implementation/FallingFoliageManualReview_20261011.md`，尚未人工签收。
- 其他并发修改导致独立Editor命令启动失败：`GamePlatformSettingsRuntime（平台设置运行模块）`DLL定向源码编译/链接成功，耗时19.55秒、`Result: Succeeded`，但插件缺少由目标完整构建生成的`UnrealEditor.modules`清单，独立Editor依旧报告模块找不到；`-DisablePlugins=GamePlatformSettings`不能覆盖其他启用依赖。本任务未篡改插件定义、伪造模块清单或关闭用户现有Editor进程。证据：`Saved/Validation/FallingFoliage/UE58_SettingsRuntimeDependencyBuild.log`、`UEValidationAPI2.log`。
- 当前主要剩余项：完成真正Editor/Client集成构建和模块清单、Niagara GPU编译、命中非零测试用例的UE Automation、实际Cook/Stage与IoStore验证、客户端操作截图/录像及GPU性能测试。E盘剩余空间偏低，执行规模较大的Cook前应确认空间及其他任务冲突。

### 2026-10-11 14:21后续证据核对

- 工程在本轮之前已有更新：`GamePlatformSettings（平台设置插件）` 的`UnrealEditor.modules（编辑器模块清单）`已于12:42生成，先前缺清单阻断不再按当前状态描述。
- `Saved/Validation/FallingFoliage/FullClientBuild_20261011.log`明确记录`Result: Succeeded`，耗时30.28秒；通过的是该次完整Client构建，不等于此后其他并发改动的最新构建。
- `CatalogAutomation2.log`含1项目录映射测试通过；`FoliageAssetRegistrationAutomation.log`含2项目录映射和主资产注册测试通过。
- 初次`FoliageNiagaraAutomation.log`中Niagara运行数据未就绪，后续`FoliageNiagaraGPU.log`以D3D12 RenderOffscreen（离屏GPU环境）重新执行同一`NiagaraAssets`测试，结果`Success`，1项测试真实执行。`MonolithNiagaraAudit.json`报告平台基础与四种项目变体均`valid=true`、`errorCount=0`、`warningCount=0`，但不替代玩家屏幕上的视觉验收。
- `PlatformVFXAutomation.log`可证明18项平台VFX自动化用例已执行，日志中的最后一批结果为Success；仍需要使用报告汇总核对全量18项是否全部成功。
- `ReviewMapWarmup.log`载明命令行编辑器执行`EditorLoadingAndSavingUtils.LoadMap`时触发引擎`World Memory Leaks`致命错误，复核之前不得将该次Review Map（审核关卡）暖机视为成功。避免在共享主编辑器状态下再次强行重试相同序列；需隔离映射加载的World生命周期再验证。
- 本轮**仍未获得**正式客户端Cook/Stage的容器清单通过证据，亦未完成角色选择/创建页面上的连续落地、风吹、扬起视觉人工签收；因此最终状态仍为未完成。

- 补充`DivineBeastsFoliageCatalogMappingTests.cpp（落叶目录映射C++测试）`在Editor目标下的单文件源码编译：`Result: Succeeded`，耗时7.74秒，日志`Saved/Validation/FallingFoliage/UE58_FoliageCatalogSingleFileBuild.log`；只证明测试源码可编译，尚未链接注册并实际执行，先前自动化仍为0项。
- 修正`Tools/Unreal/FrontEnd/UpdateCharacterPreviewStudio.py（角色预览工作室更新脚本）`：检测真实Landscape存在时，不再重新建立旧PreviewFloor/PreviewBackdrop；UE自带Python的AST语法检查通过。
- 改进`GenerateFallingFoliageSourceTextures.py（飘落物源纹理创作工具）`：`--verify（仅校验）`只使用标准库进行原PNG签名/8位RGBA/尺寸和Manifest全量SHA-256检查；仅`--apply（实际绘制）`需要numpy和Pillow。UE内置Python实际执行`FALLING_FOLIAGE_SOURCE_VERIFIED 8 2048`，不需要安装第三方库，不改动源纹理。
## 7. 2026-10-11最新工程收尾结果（优先于前述历史状态）

本段汇总本轮实际调用HWebCodex、UE5.8和Monolith产生的最终可核查证据；第5、6节保留历史过程，不再作为最新完成状态判断。

### 已通过

1. **项目目录和定义：**`Saved/Validation/FallingFoliage/Automation3/index.json`记录`CatalogMapping（目录映射）`、`PrimaryAssetRegistration（主资产扫描）`共2项成功，0失败；桃花、枫叶、竹叶和银杏的逻辑身份均映射至正确项目Definition路径。
2. **共享Niagara：**`Saved/Validation/FallingFoliage/AutomationGPU/index.json`记录D3D12真实图形RHI的`NiagaraAssets（Niagara资产就绪）`测试1/1成功；与NullRHI无图形上下文测试结果明确区分。
3. **Monolith编辑器资产检查：**已连接真实编辑器的Monolith MCP 0.23.0，五套`get_system_diagnostics(compile_first=true)`后`validate_system`结果均`valid=true`、错误0、警告0。证据`Saved/Validation/FallingFoliage/MonolithNiagaraAudit.json`。桃花近、远两组发射器皆为GPU模拟，确认绑定真实MI_DBA_Foliage_Peach材质、空中运动模块和地面风吹模块；检查源码HLSL包含贴地滑动和阵风抬升分支。
4. **平台级生命周期回归：**`Saved/Validation/FallingFoliage/PlatformVFXAutomation/index.json`记录`GamePlatform.VFX`共执行18项，16项成功、2项带警告成功、0失败，覆盖资源取消、实例句柄、时序/目录/参数以及预算。
5. **完整客户端构建：**`Saved/Validation/FallingFoliage/FullClientBuild_20261011.log`记录`DivineBeastsArenaClient Win64 Development`正式目标`Result: Succeeded`，而非仅单文件编译。
6. **正式Cook/Stage：**`D:\\DivineBeastsFoliageCook_20261011_v1`通过`BuildCookRun`，完成前端、角色工作室和Village三张地图的WindowsClient Cook、Stage、Pak，UAT报告`BUILD SUCCESSFUL`。
7. **真实资源容器：**同目录`ClientContainer.csv`来自`UnrealPak -ListContainer`，`ClientContainer.json`来自实际IoStore门禁，`Passed=true`、`RequiredCount=127`、缺失0。
8. **独立人工审核地图：**由真实UE5.8通过`Tools/Unreal/VFX/FallingFoliage/AuthorFallingFoliageReviewMap.py`生成`/Game/Development/FallingFoliage/Maps/L_DBA_FallingFoliageReview`，包含地面、方向光、天空光、相机、PlayerStart和唯一桃花NiagaraActor，已保存并重新读取地图对象。该地图仅供编辑器开发审核，**不等同于生产角色工作室中已人工通过**。
9. **静态交付门禁：**`Tests/Architecture/TestFallingFoliageDelivery.ps1`实际完成62项检查、失败0项；8张源纹理另通过UE内置Python标准库验证。

### 尚未通过，不得视为生产就绪

- **人工动态视觉签收：未完成。** Monolith独立预览工具虽返回成功，但桃花32帧全部为黑图；用平台既有CPU雨滴飞溅特效作对照也为黑图，说明该预览路径不具备有效视觉判定能力。必须在真实编辑器PIE或Cook客户端中观察空中飘落、静置、贴地滑动、阵风再扬起，并取得实际截图/视频、GPU性能和界面遮挡检查。
- **最新Editor测试DLL重新链接：未完成。** 新增`DivineBeastsFallingFoliageNiagaraValidationTests.cpp`使用`FApp::CanEverRender`区别NullRHI和真实图形RHI，但用户另一项UI任务正在运行`UnrealEditor.exe`占有`UnrealEditor-DivineBeastsPresentationClient.dll`，最新编译已通过、最终链接报`LNK1104`文件占用。不能擅自结束该进程。原先D3D12严格测试已在正式引擎中实际通过。
- **Dedicated Server最终干净Cook/Stage：本任务未执行。** 客户端专用资源通过静态排除规则检查，不把静态配置视为正式服务器发行包审计。
- **跨设备GPU性能量化：待测。** 本轮有RTX4090 D3D12脚本/就绪测试证据，但没有帧时间、GPU预算、角色界面持续运行实测。

**最新结论：工程资产、共享Niagara编译、平台逻辑回归、正式客户端构建及Cook/IoStore均已通过；因人工画面和GPU性能尚未签收，整个任务状态仍为“待最终人工验证”，不得声明全部完成。**



