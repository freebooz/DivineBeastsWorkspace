# GamePlatformVFX（游戏平台视觉特效插件）

跨游戏通用 VFX 运行框架。物理目录属于 GameFoundation（游戏平台基础层），资源挂载点为 /GamePlatformVFX/。

## 当前实现

- GamePlatformVFXClient（VFX客户端运行模块）：ClientOnly；Dedicated Server（专用服务器）不加载。
- GamePlatformVFXEditor（VFX编辑器模块）：仅负责编辑器验证和制作辅助。
- 正式只保留 GamePlatformVFXClient（VFX客户端模块，ClientOnly）和 GamePlatformVFXEditor（VFX编辑器模块）；不创建Runtime/Server空模块。
- 10种Behavior完整覆盖：Instant、Attached、Projectile、Beam、Area、Shield、Portal、Trail、World、Composite。
- 17类ContentCategory用于中立内容制作、检索与审核，不承载Gameplay规则。
- 正式运行链：GamePlatformPresentation Semantic Request（平台表现语义请求）→ VFX Provider → Catalog/Resolver → Definition → GamePlatformData异步Lease → Niagara Executor → World Instance Registry/Handle。
- Definition包含DefinitionId、PrimaryAssetId、EffectType、Parameter Schema、平台/质量变体、Fallback、PreloadAssets、Pooling/Scalability、LWC/Bounds/Lifetime、Version/Revision。
- Resolver确定性排序并拒绝同级歧义；Cache随Catalog Revision失效，不依赖数组/加载/注册/Hash顺序。
- 公共参数由Parameter Schema白名单约束；Position与Vector分离，LWC位置使用SetVariablePosition。
- Pooling优先Niagara原生组件池；Scalability/Culling优先Niagara Effect Type，平台只保留紧急实例上限。
- Handle包含Generation和弱World身份；World实例记录保存Request、Definition、LoadLease、Lifetime和State；预测/确认请求按ActivationId/PredictionKey去重。
- 已提供6个正式验证入口，并保留旧插件内部脚本作为兼容静态证据。

## 边界

本插件不得包含 MOBA 或《神兽联盟》生肖、英雄、世界项目资源。项目表现映射和美术资产应位于 DivineBeasts（神兽联盟项目层）。

当前未创建任何伪造的 .uasset/.umap。Niagara、材质、纹理、Review（人工核验）地图等二进制资产必须由 Unreal Editor（虚幻编辑器）正式创建。

当前静态验证结果：生产门禁77/77通过，Catalog/Resolver门禁19/19通过，World生命周期门禁15/15通过，Scalability门禁9/9通过，综合验证55项无失败；附件指定46份专题文档完整。

当前真实UE5.8 Editor/Client/Server编译、UE Automation、Client/Server Cook、Multi-PIE、L_VFXReview、5v5/Android性能均因Runner未配置UE_ROOT或缺少合法二进制测试资产而保持“未执行”，未以静态门禁冒充运行通过。

完整架构见本插件 Docs/ 和工作区 Docs/DivineBeastsWorkspace_Architecture_40Plugins_FourServers.md。
