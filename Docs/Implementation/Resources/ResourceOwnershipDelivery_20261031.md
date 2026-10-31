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
- 独立Editor PID41308冷读全部175目标及634条依赖边通过；175旧路径均无原生引用，三张审核地图实际Landscape已绑定新Owner材质。Cold截图材质正常，布局指纹一致；实际t.MaxFPS=0、r.VSync=0。
- 当前植物原生统一风/交互两分支、24草/12花配置，地形291节点/8函数、三质量MI和Snow ClientBridgeContract 1/1通过。最终`DeliveryVerification.json`绑定原生/构建/容器/脚本证据，不代替人工签收。

## 发布边界与验证状态

正式Client默认拒绝供应商Development/Vendor、鸟History和Village开发地形；PluginDemoClient仅精确撤销开发地形禁入，沿用既有11图白名单。专服排除规则只维护`Build/Game/ServerPresentationExclusions.json`并生成三份INI及Pak规则；世界权威地图/地形/AI未被泛化剥离。

| 门禁 | 当前结果 |
| --- | --- |
| 资源合同回归 | 10/10通过，含目标冲突、越界、Owner和归档执行阻断 |
| 资源发布容器回归 | 7/7通过，覆盖退休/样例/历史禁入、精确演示例外、真实导出及ShaderCode元数据 |
| 专服资源策略 | 35/35通过，含开发地形/历史/样例剥离及权威保留 |
| 鸟群发布策略 | 19/19通过，迁移后的历史网格/附属块仍被既有策略拒绝 |
| 地形相关最小回归 | 函数集成、包引用、V3源图/真实文件及正式软路径通过 |
| 受保护VFX兼容合同 | 8项退休作者及6项SpellPresentation合同测试通过；兼容stub保留 |
| 独立UE冷读 | 175目标、634边、三审核地图、43 Actor/ISM布局通过；85项离线+1项原生测试通过 |
| 完整Editor / Client / Server增量构建 | 全部成功；Editor证据Eligible，沿用固定增量参数 |
| 实际Client / Server / PluginDemo Cook与容器核对 | 原生Cook/Stage全部成功；Server及11图演示完整入口通过；Client新门禁修复后对相同Stage独立复核通过，原失败收据保留 |
| 新Stage运行及人工视觉 | 未执行新Stage联机准入；保留既有客户端/服务状态，人工视觉与GPU未代签 |

完整Editor构建：`a71838ba-f616-40a5-aa11-48c61ee9de8b`；完整Client/Server构建：`b0bc05c3-1410-4399-a17d-37e1ab5b6159`。实际Server Cook：`e5a833ae-9820-4eaa-8993-d956610b6265`（1459导出，本轮迁移纯表现0）；Client Cook：`f3463976-344b-448d-8453-39eb79f32c52`（2829导出，51植物+10色图）；PluginDemoClient：`d91429a8-1a24-4fa0-a089-dc68ae4f2b67`（11图、1540导出，精确引用2项审核地形，其余未强制Cook）。全部证据在工程Saved/Validation/FoundationM0对应目录。

新增持续发布门禁为`Tools/Unreal/Resources/ValidateResourceOwnershipContainer.py`，两Cook出口共用同一Manifest，枚举本次所有IoStore容器；正式拒绝退休源根、旧身份、开发样例/鸟历史，演示只允许合同中terrain_review的确切目标。客户端61项软依赖必须各有唯一、正长度的真实ExportBundleData，BulkData不能代替。

## 执行异常与恢复

地形色图第一次因CDO源码/INI软引用警示在unattended中默认取消，零目标写入；保留Cancelled日志后，经已获批准的原生可见确认完成。样例迁移后，旧空包被错误强制FullyLoad再保存，复活了旧导出并指向新包私有子对象；新目标像素/参数未变，已用原生ConsolidateAssets修复并精确清理本轮无引用Redirector。作者保存流程已改，不再保存空旧包；未禁用验证器。供应商原名称沿用原稿，其既有GP.Naming前缀提示未通过改名掩盖，本轮不宣称全工程命名审计通过。

原始Before基线不重写；Mip驻留尺寸不能替代Texture源尺寸，UE FName大小写按原生等价身份比较，源PNG哈希仍严格核验。全部Cook/Stage/备份与临时输出均留在当前工程Saved。

首次专服输入了错误MainArena叶子名，白名单在Cook之前拒绝，另用真实`L_DBA_MainArena`新RunId执行成功。首次Client总体收据Failed源于新门禁未识别原生`<ShaderCode>/ShaderCode`元数据；原生UAT、UI/鸟群/内容/施法/草地门禁都为0。补精确类型回归后，`ClientContainerRecheck/ResourceOwnership.json`重新读取同一Stage全部容器通过；两份新旧原生CSV逐字节相同，未重复Cook，未修改原失败结果。最终汇总报告明确保留该恢复链路。

一次fresh整体审查发现上述持续门禁遗漏1项Important，已按RED→GREEN补齐；85项相关离线回归全通过，未派二次复审。用户追加全项目脚本清理后，共归档7项45548字节、保留894入口；693 Python和207 PowerShell语法0错，原始/归档SHA均匹配，详见`../ScriptCleanupAudit_20261031.md`与`Tools/Archive/ScriptRetirementRegistry_20261031.json`。

## 执行者裁定

- Village绑定审核材质整体迁Owner，保持材质图原样。代价是仍不能当成跨世界通用母材质；以后复用需参数化默认纹理。
- 运行供应商闭包与样例分开，旧鸟/StyleHex保留历史。代价是源工程体积保留，正式发布必须持续执行剥离门禁。

人工视觉/GPU性能未签收，不能将本轮目录迁移标为整项目美术或性能验收通过。
