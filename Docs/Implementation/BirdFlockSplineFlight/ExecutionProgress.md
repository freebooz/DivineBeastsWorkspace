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
