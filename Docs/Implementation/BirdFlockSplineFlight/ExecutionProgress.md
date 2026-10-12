# BF-P0—BF-P8 实施进展（独立于计划与历史鸟群报告）

> 基线：2026-10-12｜当前工作区：DivineBeastsWorkspace｜源验证与真实UE资产状态分别记录。
> 注意：已有平台BirdFlock@1的历史成功证据**不能**用于本次新增Spline/Boids/VAT功能的运行验收。

## 工作包事实

| 阶段 | 代码/作者工具 | 真UE资产 | 实测/人工 |
| --- | --- | --- | --- |
| BF-P0 基线 | 已生成BaselineInventory.json及39个文件SHA目录 | 使用已有真实BirdFlock资源，未覆盖 | 新扩展未测试 |
| BF-P1 路线/配置 | 已新增平台SplineMath与项目FlightSettings、RouteActor，保留旧模式0/1/2 | 新路线蓝图作者脚本已准备 | 待UE反射类型编译、真实地图 |
| BF-P2 Niagara样条跟随 | 已编写4份GPU HLSL源、Niagara Monolith编辑器作者 | **新NM_GP_模块.uasset尚未确认存在** | GPU Spline DI绑定及实际轨迹未通过 |
| BF-P3 批次生成回收 | 已新增UDivineBeastsBirdFlockBatchComponent，复用Owner/Token | 场景蓝图/审核地图作者已准备 | 请求并发、GPU实数和600秒稳定性未验证 |
| BF-P4 Boids | 已新增有界权重策略和HLSL Steering源 | NeighborGrid3D及Niagara模块图未生成 | 不能宣称GPU Boids已经生效 |
| BF-P5 羽翼/WPO/VAT/LOD | 正式WPO资源保持原有；新增WingState HLSL和LOD材质作者工具 | 新VAT/LOD资产未实际由UE回读；可选VAT缺真实骨骼输入 | 原WPO可作旧版本回退；新动画未验证 |
| BF-P6 世界生态 | 已增加Village/OpenWorld客户端鸟群路线接入；新增3份相对锚点预设和生态地图作者 | 已有L_Village_Start主地图；OpenWorld真实地图未在Git清单发现；未向服务端地图写入纯表现资产 | 三场景真机视觉/遮挡审核待做 |
| BF-P7 演示中心 | 已移除旧ISM正式演示请求，接入正式SceneComponent/Presentation；增加BirdFlockDemoWidget与Monolith UI规格/作者工具 | 真实WBP_DBA_PluginDemoBirdFlock及SplineReview.umap待Monolith/UE生成 | 演示功能与焦点/返回需验证 |
| BF-P8 最终验证 | 新增ValidateBirdFlockSplineExtension.py、人工验收清单 | 真Editor/Client/Server构建、Cook、GPU测试、人工审美未完成 | 待集中验证 |

## 确认的安全门禁

1. 旧的正式BirdFlock DefinitionId、三种鸟类和七参数合同保持兼容；新配置默认关闭。
2. bEnableVerifiedGPUFlight（已验证高级GPU鸟群许可）初始关闭；缺新Niagara图/Schema不能误触发旧FigureEight当作Spline成功。
3. 一只鸟对应GPU粒子；不创建每鸟Actor/骨骼组件/网络复制。演示中GetActualBirdCount返回-1直到能真实GPU读回。
4. 仅由真实UE/Monolith作者制作.uasset/.umap，脚本只含作者指令，不替代编辑器成功。
5. 未获得正式OpenWorld真实地图和Scene Anchor审核时，不往服务器主地图写视觉BirdFlock。

## 最终门禁

只有完成真实资产创建与回读、Niagara GPU Spline/Boids/羽翼效果、UE Editor/Client/Server构建、自动化、Client/Server Cook、≥600秒长稳和人工签署，才能把BF-P0—BF-P8整体标为“完成”。编译失败或工具不可用必须保留原始日志和具体阻塞，不得生成假PASS。

## 2026-10-12 本轮真实构建与资产门禁结果

- **UHT（虚幻头文件工具）**：已对Editor（编辑器）目标完成本轮反射代码生成，日志记录48个代码生成文件；后续完整构建因源引擎依赖动作数过大且缺乏实际进度被停止，不能视为Editor构建通过。
- **逐文件真实C++诊断**：UE5.8 -SingleFile（定向单文件编译）与 -NoUBA（关闭远程加速）共核验11个鸟群相关源码；首次10项通过，DivineBeastsBirdFlockDemoWidget.cpp（鸟群专题UI桥接）因不存在的Misc/LexFromString.h报C1083；已替换为真实UE5.8 Misc/DefaultValueHelper.h（默认值解析头），并单独复编通过，退出码0。结论只覆盖单文件编译，不含模块链接。
- **模块构建**：三模块限定构建仍需要74项包括GamePlatformData（平台数据）和Niagara的依赖构建，在最大并发2与1两种设置下均出现队列长期无实际进度，本任务已停止对应构建，模块DLL未生成或验收。
- **编辑器验证**：现有UE Editor（编辑器）启动报告 Incompatible or missing module: DivineBeastsDemoClient（演示客户端模块缺失），未能建立用于本任务的Monolith制作会话；通过禁用DBAClient的独立作者模式尝试也未取得可信Monolith连接。插件服务注册元数据健康不代表编辑器可用。
- **并行任务**：其他会话已占用本工作空间编辑器与UBT（虚幻构建工具）进程，涉及DivineBeastsPresentationClient（项目表现客户端）和GamePlatformSurfaceClient（平台表面材质客户端）等。本轮未停止、接管这些进程，也未并行写入其UE资产。
- **严格门禁**：ValidateBirdFlockSplineExtension.py --source（源校验）7项全部通过；--strict（正式交付门禁）退出码3，缺7件实际UE资产与11项运行／人工证据。证据：Saved/Validation/BirdFlockSplineFlight/StaticExtensionGate.json（静态验证记录）。
- **资产保护**：Niagara作者改为先复制旧三种NS_DBA_BirdFlock原系统至独立Spline变体，核验Spline DI AttachParentOnly（附着父组件读取样条数据接口）后才向新变体写模块。Definition作者仅复制旧@1至新@2资产并校验旧@1哈希不变，不能误改现有三鸟种或七参数。
- **尚未完成**：新Niagara GPU图与数据接口、DataAsset@2（新版本数据资产）、Spline（样条）蓝图、WBP鸟群界面、UE地图、Client/Server Cook（客户端/服务器资源烘焙）、GPU读回、600秒实测及人工签署。高级GPU鸟群模式仍默认关闭，避免以旧FigureEight（八字轨迹）冒充新的样条群飞。

以上为本轮可复核快照，不代替其他会话之后产生的构建结果。

