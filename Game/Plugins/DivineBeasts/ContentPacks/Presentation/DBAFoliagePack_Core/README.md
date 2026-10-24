# SSLM公共植物内容包

第三层DivineBeasts纯内容插件；不新增C++模块、Actor播放器或PCG执行器。依赖方向为项目植物内容 → 平台GamePlatformPCG/GamePlatformData；无需MOBA层适配。内容包不是第四层。

用户提出的`/Game/DivineBeasts/Foliage`七类目录以插件挂载点实现，唯一正式根为`/DBAFoliagePack_Core/Foliage`，不在薄主工程另建一套同名库。

| 目录 | 职责与现状 |
| --- | --- |
| Trees | 已批准的原型树库副本；明确Prototype命名，不冒充正式树种模型 |
| Shrubs | 灌木资源目录，尚未导入模型 |
| Grass | 草类资源目录，尚未导入独立草网格；已有世界草地材质继续由原包持有 |
| Flowers | 花卉资源目录，尚未导入模型 |
| Materials | 共用植物材质和原型树材质实例 |
| FoliageTypes | 真实UE植被类型配置，引用本包树网格，含缩放、间距及剔除配置 |
| PCG | 复用平台MeshSetDefinition的真实PCG网格目录；不复制森林生成框架 |

原型树来源：`/DBAWorldPack_Village/PCG/HillsideForest/SM_DBA_Review_WideCrown`及配套简单材质，经Monolith原生复制并将全部材质引用收敛到本包。原世界资源及地图引用保留；这里是供后续复用的公共库，不自动替换已审核场景。

当前只提供客户端/编辑器装饰资产，禁止以该库决定碰撞、导航、采集、出生或战斗结果。专服Cook与Stage显式排除本包；需要玩法碰撞时另行设计权威最小资源合同。登记不等于自动运行或全包强制Cook；被批准的地图/配置引用时再纳入对应发布闭包。

资源成熟度：原型库。原生保存/重载、依赖审计记录另见`Saved/Validation/FoliagePack20261024`；正式美术、客户端Cook、GPU和人工视觉验收不能由目录或文档代替。
