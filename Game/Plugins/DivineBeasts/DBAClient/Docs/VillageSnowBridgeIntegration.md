# 神兽联盟新手村草地与雪层客户端装配规范（UE5.8）

## 归属和边界

- 第一层 `GamePlatformWeather`（平台天气插件）：负责唯一权威天气、网络快照及客户端事件，禁止客户端私下创建服务器天气。
- 第一层 `GamePlatformSurface`（通用表面材质插件）：拥有2048雪层覆盖、雪面法线等高清纹理，以及通用MPC和雪材质函数。
- 第一层 `GamePlatformVFX`（通用特效插件）：拥有近景、远景降雪及地面风吹雪粉Niagara。
- 第二层 `MobaCommon`：本功能无额外MOBA规则，不增加空模块。
- 第三层 `DBAWorldPack_Village`（新手村纯内容包）：共享Server/Client地图、55节点中性Landscape母材质（原48节点加7个草地输入节点）和三张极小默认Texture2D，地图和共享母材质禁止硬引用纯客户端资源。
- 第三层 `DBAPresentationPack_Core`（公共表现内容包）：三张2K草地扫描PBR。来源是WorldPack既有SourceArt的CC0 Grass真源，不复制另一套机制。
- 第三层 `DBAClient/DivineBeastsPresentationClient`（仅客户端）：`UDivineBeastsVillageSnowSettings`（雪层客户端软配置）及`UDivineBeastsVillageSnowWorldSubsystem`（世界生命周期与异步绑定器）。

## 运行时具体实现

1. 仅当World拥有 `/DBAWorldPack_Village/Maps/L_Village_Start` 身份、为Game/PIE且不处于Dedicated Server时启用。世界重建与后加入均按本世界WeatherClient快照同步，其他地图完全不生效。
2. 世界开始即调用Data唯一服务 `AcquireResources`（获取普通软对象租约）异步加载草地BaseColor/Normal/ORM和积雪覆盖/基础色/法线六张纹理；租约寿命严格为World，结束释放。草地加载不等待天气事件，WeatherClient通过原事件更新雪量与湿润度。
3. Landscape必须由共享地图事先保存 `M_DBA_Village_LandscapeBridge` 和 `bUseDynamicMaterialInstance=true`；客户端不使用 `EditorSetLandscapeMaterial`（仅编辑器换材质）或同步 `LoadObject`（同步资源装载）。
4. 异步成功后匹配真实材质路径，按配置语义路径而非Data规范化排序下标写入六个Texture参数。草地250厘米一重复；干燥白色乘色保留扫描源色，湿润按既有天气过渡，雪量覆盖原雪分支。每一MID六张贴图、天气标量及干燥乘色回读一致，才记录 `VILLAGE_GROUND_MID_VERIFY_SUCCESS` 和兼容的 `VILLAGE_SNOW_MID_VERIFY_SUCCESS`；无MID、材质错误或任一纹理不一致不能假报成功。
5. 只在首次绑定或新出现的Landscape上设置纹理；后续天气过渡仅更新数值参数，避免每0.1秒反复调用Texture Setter；不增Tick，按世界结束回收弱对象标识。大型动态Landscape组件再生应复核相同代理下的MID刷新语义，当前Village单Landscape已通过运行时验证。
6. 视觉天气字段不参与服务器碰撞、移动、导航和伤害结算。实时风速/风向联动、真正看见下雪、GPU透明叠加等仍需独立测试。

## 2026-10-12 苔藓/积雪复合材质：仅对主动标记的客户端岩石启用

平台 `GamePlatformSurface`（通用表面插件）持有 `M_GP_Surface_SnowMoss_PBR`（通用雪苔PBR母材质）及5张苔藓高清纹理，继续与已有 Weather（天气）8项MPC（全局材质参数集合）和VFX（客户端特效）复用，不改变服务器权威天气数值与粒子协议。项目层 `DBAPresentationPack_Core`（神兽联盟项目表现包）持有 `MI_DBA_MossSnow_Cliff`（岩石外观材质实例）。

项目客户端新增 `DivineBeastsVillageMossSurface.cpp`（Village标签式岩石表面适配），复用原世界子系统，不创建第二个玩家或天气Subsystem。其配置 `MossSnowComponentTag`（组件标签，默认 `DBA.Surface.MossSnow`）必须由项目世界/PCG产生的指定 `UMeshComponent`（网格组件）主动设置；没有标签绝不替换岩壁、地形、角色、建筑或水面。材质通过GamePlatformData的World租约软加载；资源失效保持原材质，世界结束时仅恢复仍使用本次MI的原材质槽并释放租约，避免覆盖别的系统后续材质变更。

当前正式Village根图不能保证拥有带上述标签的网格：**启用桥接能力≠场景已有苔藓可见实例**。与PCG编辑器作者工具协同时，应在生成的岩石类组件上附加稳定标签，或在后续批准的场景资产修改中明确设置；碰撞、导航与服务器Cook的结果仍由共享地图/PCG权威烘焙负责。不要将 `MI_DBA_MossSnow_Cliff` 直接硬引用到Village共享服务器地图。

新增 `ValidateVillageMossSnowIntegration.py`（项目雪苔三层静态门禁）、`ValidateMossSnowCookedContainer.py`（真实UnrealPak双端资源验收）及 `CaptureMossSnowGPUReview.py`（三态SceneCapture GPU原图审核工具）。正式项目服务器不得打包任何表面PBR、粒子、项目纯表现MI及独立审核地图；是否达到此要求必须以真实Cook+Pak报告为准。

## 编译、Cook和验收

编译：锁定UE5.8 `Build.bat DivineBeastsArenaClient Win64 Development`，本次ClientOnly代码经过真实完整客户端构建及链接；Dedicated Server目标构建后不得依赖该客户端模块。

资源：`Tools/Unreal/Weather/GenerateVillageLandscapeBridgeSources.py`（三张中性PNG生成及SHA），`AuthorVillageLandscapeBridge.py`（真实材质/地图制作），`VerifyVillageLandscapeBridge.py`（真实UE地图及依赖回读）。静态测试为`Tests/Architecture/ValidateVillageSnowBridgeIntegration.py`，真实IoStore验收为`Tests/Assets/ValidateVillageSnowCookedContainer.py`；`DivineBeastsVillageSnowBridgeTests.cpp`为原生UE自动化测试源码。

只有完成客户端和专用服务器的真实Build+Cook+Stage+UnrealPak容器验证，以及独立的实际大雪天气画面/GPU性能与用户签收，才能声明完整交付。

## 2026-10-13 本轮范围与证据边界

草地作者及只读资产检查均经Monolith，三张真实Texture2D和55节点母材质已保存；材质实际 `is_compiled=true`。两个PlayerStart、地图、地形高度、碰撞和导航未修改。默认Village鸟群由项目客户端既有入口增加为32只Seagull与24只Swallow，Niagara自身再加850/570厘米飞行高度，意图中心仅增加350/250厘米，避免旧高空鸟难以观察。

已观察旧DLL真实PIE的RED：草地Albedo仍为共享默认、鸟群请求为24+0。新C++尚需真实构建和运行，不能把资产保存或代码差异当客户端功能通过。HWebCodex四个长驻任务占满执行名额；新构建排队后已取消（没有启动编译）。当前人工客户端与服务器仍为上一发布包。完整状态见 `Docs/Implementation/VillageAmbientImplementationPlan_20261013.md`，本轮未完成发布不提升此前雪桥验收结论。

新原生回归包括真实MID语义绑定、缺纹理/关闭、真实Data异步租约取消/错误类型、双群布局与切图清理。静态门禁和临时CSV单元测试不代替这些尚待运行的原生用例。
