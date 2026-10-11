# 神兽联盟写实积雪三层架构与插件归属
## 2026-10-11 补充：真实运行边界与资产审计

- 第一层原生资产`M_GP_SnowCover_Detailed`不复制为项目实现，实际56节点遮罩由`GamePlatformSurface`拥有，参数`SnowCoverageShiftScale=0.4`只用于可配置积雪视觉。Niagara`NS_GP_Weather_Snow_Detailed`由`GamePlatformVFX`拥有，近/远景速度、数量及世界空间贴地雪粉依天气表现语义驱动；不进入服务器玩法权威。
- 第三层项目`DBAPresentationPack_Core/Materials/Snow/MI_DBA_Snow_Detailed`只覆盖项目材质参数，不能升级为平台算法。本轮`L_DBA_SnowReview`属于开发地图，世界天气审核Actor默认HeavySnow/0秒，不能作为正式新手村的游戏入口。
- `Tools/Unreal/Weather/UpgradeSnowCoverBlend.py`负责原生UE材质迁移，`ConfigureSnowReviewEnvironment.py`负责审核地图配置，`InspectVillageSnowMaterialIntegration.py`仅做只读世界清点，`Tests/Assets/ValidateSnowCoverageResponse.py`做数值回归，`VerifySnowNativeGraphsMonolith.py`通过正确工程中的Monolith验证实际Material/Niagara连线，`ValidateSnowCookedContainer.py`以UnrealPak输出比对真实Cook闭包。
- 只读Village根地图检查：8个已加载Actor、1个Landscape；其LandscapeMaterial为空，已加载静态网格材质插槽0，Surface/项目雪材质显式引用0。**这是所加载根关卡的样本**，不能推断全部流送区域不存在植被/材质，不能据此直接替换共享Village的服务器资源。需要客户端独立附加/恢复雪表面材质而不改变服务器可烘焙闭包。
- 双端正式Build.bat检查与Cook/Stage/IoStore验证记录于`Docs/Implementation/SnowSurfaceContinuationValidation_20261011.md`；最终人工可视、双客户端和GPU性能仍未签收。



第一层 GamePlatformSurface：通用写实积雪母材质、世界空间雪层遮罩、MPC参数、雪面源纹理；GamePlatformWeather保留已有服务器权威天气事实；GamePlatformVFX提供通用近远景降雪和独立地表雪粉Niagara。不得同步单个粒子、不能把视觉积雪当玩法权威。

第二层 MobaCommon：不具有本通用表面雪材质的业务规则，不新增模块。

第三层 DBAPresentationPack_Core：跨地图可复用的神兽联盟积雪材质实例MI_DBA_Snow_Detailed。项目开发审核地图L_DBA_SnowReview位于Game/Development/Snow，不修改正式Village地图。

客户端FrontEndClient Cook显式收录雪源Texture2D和项目MI；DedicatedServer配置排除所有纯客户端雪表面纹理、材质、Niagara和开发审核地图。有关实际资产、工具、测试及人工验收状态见Docs/Implementation/SnowSurfaceAndVFXImplementationPlan_20261011.md。
