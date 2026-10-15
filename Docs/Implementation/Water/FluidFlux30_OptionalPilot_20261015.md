# SSLM Fluid Flux 3.0 可选水体试点执行记录（2026-10-15）

## 目的与当前结论

在 UE5.8（虚幻引擎5.8）SSLM 工程评估 Fluid Flux 3.0（第三方浅水流体模拟资产）可行性，**不替换**已建立的 GamePlatformWater（平台水体）、GamePlatformSurface（表面材质）、GamePlatformVFX（视觉效果）、GamePlatformWeather（天气）、GamePlatformPCG（场景生成）。

此试点只适用于 DBAFrontEndPack（项目角色选择/创建内容包）的 L_DBA_CharacterStudio（独立角色工作室），不扩展到 Village（新手村）、OpenWorld（开放世界）或 MainArena（竞技地图），也不参与正式碰撞、游泳、浮力、伤害、网络同步。先测不破坏现有正式水体。根据现有 P4—P8 边界，任何扩大适用范围都需要独立架构审查。

**本次已核实：** GamePlatformWater 三模块、34项现有原生水体资源与源PNG静态门禁通过；尚未在预设安装路径发现正版 FluidFlux.uplugin（插件描述文件）。不代表用户未购买，也不能据此证明 UE5.8 不兼容。插件下载与安装、真实GPU演示和双端Cook都尚未完成。

## 三层归属与开关

- GamePlatformWaterRuntime（基础平台水域逻辑）保持现有服务器深度、区域与流向真源，不引用 Fluid Flux 或任何 GPU 计算。
- GamePlatformWaterClient（基础客户端水体表现）保留官方 Water（湖河水体）、中立事件、统一表现请求；不链接 Fluid Flux。
- MobaCommon（通用MOBA层）没有额外水体职责，不新增空模块。
- DivineBeastsPresentationClient（SSLM项目客户端表现）继续通过 DivineBeastsCharacterStudioWaterPolicy（角色工作室水体方案策略）进行可选资格判断；不直接创建第三方流体对象。后续确实安装时，才由独立受控的客户端表现适配实现实际实例生命周期。
- 真正的授权资源和编辑器资产只允许放入第三方许可允许的私有独立内容包或合规插件目录，不复制到平台通用插件，也不导入 Dedicated Server（专用服务器）Cook。

项目开关缺省关闭；**不是**在默认配置中偷偷加入开启参数。只有完整验收后，测试人员才允许在专用开发配置加入：

~~~ini
[DivineBeasts.CharacterStudio.Water]
bFluidFlux3PreviewVerified=true
~~~

bFluidFlux3PreviewVerified（Fluid Flux 3.0预览验收开关）必须由真实 UE5.8 兼容、着色器、性能、可撤销加载/卸载和许可核对均通过后批准打开。代码仍额外要求桌面平台、非命令行、非专用服务器、插件已启用、明确偏好、高画质、真实本地单人角色工作室关卡。任何条件不满足返回 Native（原生水面）。

**重要：** 目前返回 FluidFluxEligible（允许尝试流体）也不代表 GPU 求解器已创建；本轮没有创建任何带 Fluid Flux 类引用的 UObject、蓝图或 .uasset（虚幻资源）。

## 本轮文件与质量门禁

- DivineBeastsCharacterStudioWaterPolicy.cpp/.h（角色水效策略源码）：新增显式正式审核资格与失败关闭门禁。
- DivineBeastsFluidFluxPreviewPolicyTests.cpp（原生自动测试）：验证空舞台、高低画质与开关未批准等失败关闭条件。
- Tests/Architecture/ValidateFluidFluxOptionalIntegration.py（三层依赖静态审查）：默认原生水体必须安全，可用 --require-vendor（要求第三方插件）在正式安装前置门禁失败。--vendor-uplugin（插件描述文件路径）可指向用户已授权安装位置。
- Tests/Performance/Water/CompareFluidFluxPilot.py（水体性能A/B对比）：严格要求同场景、画质、分辨率和逐帧可核对输入；只比较输入，不自动编造 GPU 读数。
- Tests/Performance/Water/TestCompareFluidFluxPilot.py（比较算法自测）：人工合成CSV验证通过、超预算失败、样本不足失败、非法数值拒绝。**这些测试不是 UE5.8 显卡实测。**

### 正式静态审查命令

~~~powershell
python Tests/Architecture/ValidateFluidFluxOptionalIntegration.py
python Tests/Architecture/ValidateFluidFluxOptionalIntegration.py --vendor-uplugin "合法安装目录/FluidFlux.uplugin" --require-vendor
python Tests/Architecture/ValidateGamePlatformWaterAssets.py --require-review-map
python -m unittest discover -s Tests/Performance/Water -p TestCompareFluidFluxPilot.py -v
~~~

## 真正验证需要额外执行的步骤

1. 使用用户授权的 Fab（虚幻资产商城）下载并安装原版，检查产品许可、资产版本、第三方插件内部模块实际名称、UE5.8 支持声明、必要依赖和缺失资源。不得绕过登录或下载授权。
2. 在一个独立的 UE5.8 审核工程或可隔离工作树中激活 Fluid Flux，记录 Editor（编辑器）完整编译、Shader（着色器）无错误和客户端独立打包结果。**不能**把“插件描述文件存在”写成引擎兼容验收成功。
3. 只在角色工作室的**测试副本**中对照现有原生水景进行 A/B（原生方案与 Fluid Flux 方案）测试，不改正式新手村地形、PCG、水材质及角色创建流程。
4. 同一台机器、同一分辨率、同画质、同视点、相同时间/天气和相同角色；每种模式完成预热和不少于120帧有效采集，记录 UE Insights（虚幻性能分析器）、stat unit（引擎帧时间）、stat gpu（GPU时间）、RHI（渲染硬件接口）显存与原始日志。禁止凭画面流畅感估算 FPS。
5. 将已核实的实测数据标准化为 CSV 列：Scene（场景）、Mode（方案 Native/FluidFlux）、Quality（档位）、Resolution（分辨率）、Frame（帧号）、GPUms（GPU帧时）、GameThreadMs（游戏线程）、RenderThreadMs（渲染线程）、VRAMMiB（显存兆字节）。每种模式至少150帧，默认丢弃前30帧，获得至少120帧。
6. 使用对比命令进行首轮**项目候选预算**门禁：

~~~powershell
python Tests/Performance/Water/CompareFluidFluxPilot.py --input "Saved/Validation/FluidFlux/CharacterStudio_A_B.csv"
~~~

7. 首轮 High（高画质）的 GPU P95（水体场景第95百分位GPU帧时）新增预算为不超过2.0ms，Fluid Flux 完整场景 GPU P95 不超过16.667ms；Medium（中）为1.5ms、Low（低）为1.0ms、Cinematic（电影档）为3.0ms。这些是项目初始目标，非插件官方声明。脚本同时记录 CPU 游戏/渲染线程与显存 P95，但这些字段尚无已批准的额外预算，不据其数值声称优化完成。
8. 进入与退出工作室反复至少100次，核对真实渲染目标和组件回收、角色预览不闪烁、世界卸载无悬挂异步回调。进行断开/重连、客户端失败降级以及 Dedicated Server 内容剥离检查。
9. 取得真实引擎测试与性能原始证据后，更新本记录并评估是否创建真实供应商适配内容包。在此之前保持开关关闭。

## 2026-10-15 实际执行校验记录

- `ValidateGamePlatformWaterAssets.py`（通用水体静态资产校验）返回34项存在、0问题；`--require-review-map`（要求审核地图文件存在）同样通过。**这两项都没有打开UE编辑器或确认水面可见。**
- 工作空间检查发现 `Game/Content/Development/Water/Maps/L_GP_WaterReview.umap`（P4水体审核地图）存在，文件大小为6424字节；不据此声明地图配置、蓝图、Shader或视觉验收完成。
- `ValidateFluidFluxOptionalIntegration.py`（Fluid Flux可选依赖审查）普通模式通过，严格 `--require-vendor`（要求真正安装）按预期拒绝，未找到 `FluidFlux.uplugin`（正版插件描述）这一必要安装证据。对UE引擎Marketplace与项目插件的四个常用精确安装位置实际检查均不存在。
- `TestCompareFluidFluxPilot.py`（性能比较单元测试）4项通过；数据来自临时合成夹具，只验证比较逻辑，**GPU真实测试仍为0帧**。
- Python源文件语法检查、相关源码 `git diff --check`（差异格式检查）通过。
- UE5.8 第一次 `DivineBeastsPresentationClient`（项目客户端表现模块）定向编译：收据 `Saved/Validation/FoundationM0/2468a442-716d-4bcf-b7a3-5c0ee1ffa4da/Build`，退出码6；本次水体策略文件因 UPackage（包对象）完整类型未包含而报C2027，同时其他任务的 VillageWolf（新手村狼）源码出现C4458，已补齐本次水体策略的 `UObject/Package.h`。
- 第二次同目标真实编译：收据 `Saved/Validation/FoundationM0/2c548419-c54c-4f06-84b4-d211f7d1226f/Build`，退出码6；水体策略源文件编译动作完成且不再报错，剩余报错均位于独立任务 `DivineBeastsVillageWolf.cpp` 第41、71、83、89行的C4458。未修改其他任务的狼怪物源文件。因此**尚不能认定本次项目模块完整构建通过，更不能认定Fluid Flux运行通过**。

## 状态和阻塞

- **已完成：** 现有 Water 架构核查、客户端选型门禁增强、静态验证器与性能比较工具源码和自测。
- **受控阻塞：** 当前没有可使用的 Fluid Flux 正版二进制资源与已验证的 UE5.8 接口；正式试点关卡、Shader、GPU、客户端 Cook 和双端验收均待执行。
- **独立注意：** 当前主工作树另有并行任务、未提交修改和正在运行的作业。本任务不得清理/覆盖这些工作成果；已有水体 P4 审核地图文件可能与旧文档完成度不同，仍需 Editor 真正回读后才能修正该历史记录。
