# BirdFlock（通用鸟群VFX）

版本：2026-10-11｜位置：GamePlatformVFX（平台层，唯一视觉特效插件）  
状态：客户端原生代码和鸟类模型/纹理/材质已创建并经定向检查；完整编辑器蓝图、Niagara与关卡验收尚未完成，不得宣告可发布。

## 一、系统定位与可复用边界

本功能复用现有 `GamePlatformVFXClient（VFX客户端模块）`，不新建同领域插件、不在 MobaCommon（MOBA通用层）建立环境鸟群实现。平台公共源码不得了解任何具体项目的海鸥、燕子、雄鹰身份。项目实际物种资源归属 `DBAPresentationPack_Core（神兽联盟公共表现内容包）`。

现阶段客户端环境布景使用一套 `AGamePlatformBirdFlockActor（批量鸟群可视演员）` + `UInstancedStaticMeshComponent（实例化静态网格组件）` + `UGamePlatformBirdFlockProfile（鸟群配置DataAsset）`。全部鸟类共享通用飞行数学，鸟种只改变预设持有的网格、材质、速度、数量上限等数据。材质以顶点色 Alpha 决定翼尖权重，经世界位置偏移 WPO 完成 GPU 振翅。各个鸟只用实例矩阵更新位置/方向，不创建每鸟Actor，不注册动态碰撞，不做网络复制。

**重要边界**：本次完成的批量网格方案是可运行的客户端通用鸟群表现机制，但**不是已实现的 Niagara GPU 粒子鸟群资产**，亦未接入正式 `GamePlatformPresentation → GamePlatformVFX DefinitionId → Niagara Executor` 请求/租约链。项目需求如要求真实 Niagara 发射器和动态表现事件驱动，仍须完成对应编辑器模块与真实资产制作。不得将 ISM 原型或空 NiagaraSystem 冒充正式 GPU Niagara 实现。

## 二、配置接口

| 参数 | 类型与默认 | 功能与约束 |
| --- | --- | --- |
| BirdProfile（物种预设） | UGamePlatformBirdFlockProfile | 指定项目模型、材质以及飞行参数；更换即切换鸟种 |
| BirdCountOverride（数量覆盖） | int32，-1 | -1采用预设数量；0关闭；1..512合法，超过硬上限返回失败 |
| QualityFraction（画质数量比例） | float，1.0 | 0..1，0直接不生成可视实例 |
| FlockSeed（随机种子） | int32 | 分布可复现；不用于服务器玩法随机 |
| FlightMode（飞行模式） | Orbit/Traverse/FigureEight | 环绕/直飞掠过/八字盘旋；不修改服务器导航 |
| RadiusCm（飞行半径） | float，1500 | 半径范围200..100000cm |
| FlightHeightCm（相对高度） | float，800 | 相对Actor锚点高度，按cm配置，需人为布置避免穿模 |
| CruiseSpeedCmPerSecond（巡航速度） | float，240 | 10..5000cm/s |
| WindInfluenceCm（风力最大扰动） | float，95 | 外观扰动强度0..2000cm，风向来自实例；不进行无限积分 |
| UpdateRateHz（位姿刷新Hz） | float，20 | 5..60Hz，GPU材质振翅频率独立 |
| Min/MaxDrawDistanceCm（渲染裁剪距离） | int32，0/25000 | 限制实例渲染距离，需要实际GPU验证 |

实际实例数量公式：`floor(min(RequestedCount, SpeciesMaxCount) * QualityFraction)`，请求负数（-1默认哨兵除外）、超过512、非有限质量值均被拒绝。预设非法时清除旧群，不会继续渲染过期鸟类。

蓝图可直接调用通用Actor接口：`SetBirdProfile（切换预设）`、`SetBirdCount（更改鸟数）`、`SetQualityFraction（改变质量档）`、`SetWindDirection（设置局部风向）`、`RefreshFlock（立即刷新）`、`GetEffectiveBirdCount（读取当前实例数）`。

## 三、资源规范和目录

- `GamePlatformVFX/Content/Environment/BirdFlock/Materials/M_GP_BirdFlock_Master.uasset`：平台级唯一通用母材质；默认参数仅依赖Engine通用纹理，不允许硬依赖具体游戏项目。
- `DBAPresentationPack_Core/SourceArt/BirdFlock/DBA_BirdFlock_Models.blend`：Blender 5.2可编辑源，带3个UV低面数鸟类，顶点色RGB区分表面，Alpha为翼尖变形权重。
- `SourceArt/BirdFlock/Meshes/SM_DBA_Bird_{Seagull,Swallow,Eagle}.fbx`：原始米制3D网格。模型 X 为前向、Y 为翼向、Z 为上。
- `SourceArt/BirdFlock/Textures/T_DBA_Bird_*_Albedo_Source.png`：512²柔和低对比羽色原图，无刻意砂砾/颗粒纹。
- `Content/VFX/Environment/BirdFlock/Meshes/SM_DBA_Bird_*_Cm.uasset`：UE5.8正确厘米尺寸网格，需使用此后缀资源。早期无后缀的3个测试网格因导入时重复放大100倍，仅保留为历史失败证据，不得引用/用于运行；待确认无引用和编辑器空闲后受控清理。
- `Content/VFX/Environment/BirdFlock/{Materials,Textures}`：三套实际材质实例和三套正式导入纹理。
- `Content/VFX/Environment/BirdFlock/{Profiles,Blueprints}`：计划中真实物种DataAsset与统一鸟群蓝图；待编辑器目标编译解锁后创建，不得将生成脚本误称作已生成资产。
- `Tools/Blender/BirdFlock` 与 `Tools/Unreal/VFX/BirdFlock`：源资产生成、正式UE导入/回读、鸟种预设、统一蓝图、隔离审核场景工具。
- `/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview`：隔离人工审核关卡目标；当前**未生成**，不能引用虚构的.umap。

## 四、资源生产操作

1. 调用锁定 Blender 5.2 的 `GenerateBirdFlockModels.py（鸟类模型源生成脚本）`，从源生成FBX、PNG、blend和源资源SHA-256清单。重新生成前必须先确认不覆盖人工修改过的Blender源资源。
2. 使用锁定 UE5.8 `UnrealEditor-Cmd.exe（编辑器命令行）` 的 `-ExecutePythonScript` 参数执行 `AuthorBirdFlockAssets.py（实际资源导入与材质创建脚本）`，明确设置 `BIRD_FLOCK_AUTHOR_MODE=apply`。
3. 运行 `ValidateBirdFlockAssets.py（只读资产回读校验）`。三个鸟种的最大网格半宽须在30..300cm，资源类型分别为真实UStaticMesh/Texture2D/MaterialInstanceConstant，材质父类统一。
4. 在 UE 编辑器源模块允许编译后执行 `AuthorBirdFlockProfilesAndBlueprint.py（鸟群预设和蓝图作者）`，环境变量 `BIRD_FLOCK_PROFILE_MODE=apply`，应生成3个DataAsset和1个Blueprint。
5. 之后执行 `AuthorBirdFlockReviewMap.py（隔离关卡作者）`，环境变量 `BIRD_FLOCK_REVIEW_MODE=apply`，应新建独立关卡，三个鸟群Actor分别使用海鸥32、燕子72、雄鹰5的默认演示值，不改动新手村及角色选择创建地图。
6. 单独完成真实 Niagara GPU 发射器、物种Mesh Renderer以及与统一 VFX Definition/Presentation 注册连接，验证后方可将正式场景连接为事件驱动链路。

所有Python UE命令行的退出码0只说明进程结束；脚本异常有可能仍为0。必须同时检查 `LogPython: Error`、`LogEditorPythonExecuter: Error` 和输出的 `*_OK` 成功标记。

## 五、场景表现建议

- Village（新手村）湖面：海鸥低密度环绕和逆风滑翔。放置于水面上空，依照真实湖面高度设Actor锚点。
- 桃林古树：燕子群采用八字往复和短程掠过。若将来加入触发惊鸟，需从客户端表现事件驱动刷新预设，不让视觉鸟群碰撞影响游戏逻辑。
- OpenWorld（开放世界）高崖：雄鹰使用高海拔、大半径、低密度的滑翔轨迹。
- CharacterSelect/CharacterCreate（角色选择/创建场景）：远景少量鸟群，必须避让角色主体与按钮区域；不能让视觉表现控制权威流程。
- 一般不在 MainArena（主竞技场）默认开启环境鸟群，以免增加PvP画面噪声和性能负担。

## 六、性能与验收

使用 Niagara Debugger/Unreal Insights/RenderDoc（粒子分析/性能分析/渲染采样工具）结合实际目标设备。现阶段仅有ISM + GPU WPO，无法把它的CPU实例循环结果当作Niagara GPU模拟预算。单批不超过512，默认20Hz低频更新、相机最近渲染标记跳过非可见矩阵更新、ISM实例裁剪和World EndPlay清理；这些是实现逻辑，尚无真机帧耗实测。

验证顺序：
- Client Win64 Development：检查UHT + C++ + 链接。
- Editor Win64 Development：必须先让当前正在运行的Live Coding会话安全结束；不强行关闭其他开发任务的编辑器。
- Server Win64 Development：检查没有带入ClientOnly鸟群代码或纯项目鸟类资源。
- 资产重载：按正确厘米尺寸回读10件资产；不将脚本文件数量冒充uasset数。
- 客户端自动化实测：已用Win64 Development Client的NullRHI模式运行 `GamePlatform.VFX.BirdFlock.CountAndSeed`（数量/种子）及 `GamePlatform.VFX.BirdFlock.ActorInstanceLifecycle`（真实Actor实例数量/销毁），`Game/Saved/Automation/BirdFlockClientActor/index.json` 报告 **2/2 Success，0 Failed**。此测试不覆盖真实物种资产实例化、Niagara GPU编译或UE人工视觉效果。
- 功能验证：蓝图切换鸟种、数量0/1/32/128/512、切换风/路径、离开世界后无残留。
- 人工视觉：审核Blender预览和UE真实关卡，确认羽色、层次、形变、翼尖频率、合理朝向、不穿地面/水面/建筑物、不会遮挡角色和UI；主观审美必须由人确认。

## 七、未完成与限制

- Editor目标编译受到运行中的其他 UE5.8 Editor Live Coding 会话保护锁阻断，不能在其他任务仍编辑ZodiacSanctuary时强制退出进程。
- 未创建真实鸟群Blueprint/DataAsset/测试地图、未执行编辑器自动化、尚未制作专用 Niagara GPU 鸟群 Emitter/System/EffectType，亦未建立正式 Presentation Definition 映射。
- 未完成 Village/OpenWorld/CharacterSelect/CharacterCreate 正式场景接入和人工UE画面验收。
- 扩大近景质量仍需更精细的鸟类网格/羽毛贴图和姿态动画，当前低面数方案仅适合远景与中远景环境。
- 后续编辑器解锁后继续：编译 Editor → UE资产作者脚本 → 自动化测试 → Niagara GPU与正式表现分发 → 审核关卡 → 场景接入 → Cook/性能 → 人工验收。
