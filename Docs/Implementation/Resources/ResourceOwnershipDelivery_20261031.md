# SSLM资源归属整改交付记录（2026-10-31）

用户批准按资源目录审查报告整改并要求保留现有效果。当前范围为同工程175包的Monolith原生迁移、依赖收敛、历史/样例隔离与现行文档更新；不新增插件、模型、材质配方或运行机制，不改变正式Village地形/布局或当前鸟群。

## 当前归属

| 数量 | 当前目录 | 用途 |
| --- | --- | --- |
| 51 | `/DBAFoliagePack_Core/Vendor/PN_OpenWorldFoliage` | 草花网格、材质、纹理和交互MPC完整运行闭包 |
| 40 | `/Game/Development/Vendor/PN_OpenWorldFoliage` | 原供应商Showroom、ColorExamples与UE4样例 |
| 12 | `/DBAWorldPack_Village/Development/Terrain/Production10` | 三份世界绑定地形母材质及九份实例，仅审核 |
| 10 | `/DBAPresentationPack_Core/Textures/Terrain/Production10` | 公共地形色彩图，雪地软引用同步 |
| 7 | `/DBAPresentationPack_Core/Development/History/BirdFlock/Meshes` | 旧鸟网格归档，当前Flight不变 |
| 55 | `/Game/Development/Vendor/StyleHex_Studio` | 原始供应商样例及内部引用 |

路径唯一真源：`Tools/Unreal/Resources/ResourceOwnershipManifest.json`。主机/编辑器作者、INI与当前地形/草作者均同步；SourceArt原稿、许可与历史审核记录保留当时事实。公共植物包现有102项（40网格），README已纠正原型阶段说明。旧鸟首版apply入口已退休，当前Flight作者保持唯一入口。

## 原生与保效证据

证据根：`Saved/Validation/ResourceOwnershipRemediation20261031`。

- 原始4605资产/外置包已完整备份，2758929913字节；备份只复制，真实迁移/引用保存/重定向清理均由Monolith调用UE AssetTools执行。
- 原生175目标均是真实原类型对象，175旧源路径不存在。公共植物→主工程及公共表现→世界包的155条归属问题已降为0；既有兼容重定向未作为本轮清理目标。
- 35张原生导出PNG哈希一致，覆盖源像素/尺寸及颜色空间/压缩；76网格OBJ的顶点/法线/UV/面指纹一致，LOD顶点数、Bounds和材质槽一致；材质/实例参数及父引用保持一致。
- 当前20项平台/项目鸟群资源字节一致；七项历史网格零原生引用。Village43 Actor及全部ISM实例变换指纹一致。
- `Village.Before.png`拍摄时Shader尚未就绪，呈默认棋盘材质，不能作为视觉一致的前图；保留该失败证据。`Village.After.png`为已加载材质的真实当前画面，不代填人工审美验收。
- 原生独立进程冷读、构建与Cook结果执行中，以下交付状态须待实测补齐。

## 发布边界与验证状态

正式Client默认拒绝供应商Development/Vendor、鸟History和Village开发地形；PluginDemoClient仅精确撤销开发地形禁入，沿用既有11图白名单。专服排除规则只维护`Build/Game/ServerPresentationExclusions.json`并生成三份INI及Pak规则；世界权威地图/地形/AI未被泛化剥离。

| 门禁 | 当前结果 |
| --- | --- |
| 资源合同回归 | 8/8通过，含目标冲突、越界、Owner和退休作者阻断 |
| 专服资源策略 | 35/35通过，含开发地形/历史/样例剥离及权威保留 |
| 鸟群发布策略 | 19/19通过，迁移后的历史网格/附属块仍被既有策略拒绝 |
| 地形相关最小回归 | 函数集成、包引用、V3源图/真实文件及正式软路径通过 |
| 独立UE冷读 | 待执行 |
| 完整Editor / Client / Server增量构建 | 执行中 |
| 实际Client / Server / PluginDemo Cook与容器核对 | 待执行 |
| 新Stage运行及人工视觉 | 待验证；不沿用旧包启动作为本轮通过 |

## 执行异常与恢复

地形色图第一次因CDO源码/INI软引用警示在unattended中默认取消，零目标写入；保留Cancelled日志后，经已获批准的原生可见确认完成。样例迁移后，旧空包被错误强制FullyLoad再保存，复活了旧导出并指向新包私有子对象；新目标像素/参数未变，已用原生ConsolidateAssets修复并精确清理本轮无引用Redirector。作者保存流程已改，不再保存空旧包；未禁用验证器。供应商原名称沿用原稿，其既有GP.Naming前缀提示未通过改名掩盖，本轮不宣称全工程命名审计通过。

原始Before基线不重写；Mip驻留尺寸不能替代Texture源尺寸，UE FName大小写按原生等价身份比较，源PNG哈希仍严格核验。全部Cook/Stage/备份与临时输出均留在当前工程Saved。

## 执行者裁定

- Village绑定审核材质整体迁Owner，保持材质图原样。代价是仍不能当成跨世界通用母材质；以后复用需参数化默认纹理。
- 运行供应商闭包与样例分开，旧鸟/StyleHex保留历史。代价是源工程体积保留，正式发布必须持续执行剥离门禁。

人工视觉/GPU性能未签收，不能将本轮目录迁移标为整项目美术或性能验收通过。
