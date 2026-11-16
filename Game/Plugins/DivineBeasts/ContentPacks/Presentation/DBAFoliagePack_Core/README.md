# SSLM公共植物内容包

第三层DivineBeasts纯内容插件；不新增C++模块、Actor播放器或PCG执行器。依赖方向为项目植物内容 → 平台GamePlatformPCG/GamePlatformData；无需MOBA层适配。内容包不是第四层。

唯一挂载点为`/DBAFoliagePack_Core`，薄主工程不持有公共植物依赖。2026-10-31原生AssetRegistry共102项：40 StaticMesh、29 Texture2D、18 MaterialInstance、5 Material、3 MaterialFunction、3 FoliageType、2 LandscapeGrassType、1 MeshSetDefinition、1 MaterialParameterCollection。原51项本包资源与本轮迁入的51项供应商运行闭包分别可追溯。

| 目录 | 职责与现状 |
| --- | --- |
| Trees | 原型宽冠树与WeepingWillow柳树；原型身份保留，不冒充正式树种 |
| Shrubs | 灌木资源目录，尚未导入模型 |
| Grass | BostonFern、WesternSwordFern及FabOpenWorld的项目全局风材质、草/花GrassType |
| Flowers | 独立项目目录尚无模型；当前花网格归Vendor运行闭包 |
| Materials | 共用植物材质和原型树材质实例 |
| FoliageTypes | 真实UE植被类型配置，引用本包树网格，含缩放、间距及剔除配置 |
| PCG | 复用平台MeshSetDefinition的真实PCG网格目录；不复制森林生成框架 |
| Vendor/PN_OpenWorldFoliage | 36项原始草/花网格及Materials/Textures完整运行闭包共51项，包含交互MPC；保留供应商叶子名、尺度、几何与像素 |

原型树来源：`/DBAWorldPack_Village/PCG/HillsideForest/SM_DBA_Review_WideCrown`及配套简单材质，经Monolith原生复制并将全部材质引用收敛到本包。原世界资源及地图引用保留；这里是供后续复用的公共库，不自动替换已审核场景。

当前只提供客户端/编辑器装饰资产，禁止以该库决定碰撞、导航、采集、出生或战斗结果。专服Cook与Stage显式排除本包；需要玩法碰撞时另行设计权威最小资源合同。登记不等于自动运行或全包强制Cook；被批准的地图/配置引用时再纳入对应发布闭包。

供应商原始ColorExamples、Showroom、UE4_ThirdPerson共40包归`/Game/Development/Vendor/PN_OpenWorldFoliage`，不属于公共运行库，禁止正式Cook。源图、FBX/ST9及许可保留原来源记录，不回写历史导入事实。当前柳树与两种蕨类沿用既有SourceArt及导入证据；Vendor命名沿用原稿。

当前归属审计已消除公共植物包对`/Game`的反向依赖。早期原型库证据仍见`Saved/Validation/FoliagePack20261024`；本轮迁移、回读、构建、Cook与人工验收的分别状态见`Docs/Implementation/Resources/ResourceOwnershipDelivery_20261031.md`，备份及原生证据归工程`Saved/Validation/ResourceOwnershipRemediation20261031`。目录成熟度、Cook成功与人工视觉认可不能互相替代。
