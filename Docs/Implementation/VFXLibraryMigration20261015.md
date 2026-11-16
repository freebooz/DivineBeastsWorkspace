# SSLM视觉库迁移与1号治疗接入（2026-10-15）

用户指定源为本机 `D:/GameVFX`。迁移采用锁定UE5.8、Monolith 0.23和编辑器原生AdvancedCopyPackages，目标所有者为既有 `DBAPresentationPack_Core`，不新增VFX运行框架或插件身份。源工程、源码、缓存与二进制未移动或删除。

## 范围和清单

完整迁入1170份视觉资产、42个Niagara系统，以及588份原始SourceArt文件。视觉资产包含177份Material、349份MaterialInstanceConstant、434份Texture2D、14份StaticMesh、122份NiagaraScript、9份NiagaraEmitter、23份NiagaraEffectType和42份NiagaraSystem。目录保留原库分类；源Game资源位于 `VFX/Library/Game`，Demo/Showcase中的视觉依赖也纳入闭包。演示关卡、UI、旧Catalog类和旧ModularGameVFX播放框架不作为视觉资源复制。

- [完整资产清单及源哈希](../../Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Docs/ImportedVFXAssets.json)记录1170个源包、目标包、类与源SHA256。
- [42系统及平台定义清单](../../Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Docs/ImportedVFXCatalog.json)记录稳定逻辑身份、真实路径和逐系统编译状态。
- 42份库定义仅用于有界开发预览，采用Instant定义、10秒上限；未把它们无条件注册为角色技能或生产目录。具体投射、光束、区域、状态玩法接入仍须选择对应平台行为及明确参数白名单，不将预览定义当作权威实体。
- 原工程23条历史CSV保存在 `Saved/Validation/PetalBloomIntegration20261015/OriginalCatalog.csv`，它不是本次42系统的权威清单。SourceArt位于内容包的 `SourceArt/ImportedLibrary`，不进入游戏Cook。

运行时仍为中立语义请求 → GamePlatformPresentation → GamePlatformData定义/租约 → GamePlatformVFX唯一执行器。未增加Gameplay至Niagara的依赖，未把项目美术放进平台层。导入库定义被Data扫描发现，Cook只发布实际装配的治疗闭包；本次客户端容器包含29个Library包，未强制发布整个素材库。

## 原始花瓣治疗

1号技能使用原始 `/DBAPresentationPack_Core/VFX/Library/07_Status/NS_Nature_PetalBloom_Aura`，保留9层花瓣、光环、裙摆、雾和花粉。未制作新的树叶。源默认绿色配色亦保留。

真实治疗定义为 `/DBAPresentationPack_Core/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_HealingPetalBloom`，逻辑身份为 `presentation.dba.development.magic.healing.impact@1`。该Attached定义沿已有开发魔法装配登记并Cook，附着受疗者根部；六秒参数、6.2秒生命周期上限和单一Float白名单管理清理，不引入第二套播放器。

12个既有Primary能力和UIProfile原位迁移，保持身份、输入和图标。治疗有效选中友方20%最大生命；无有效选中意图时自疗；冷却10秒、不耗气势。新手村玩家均可受疗，竞技按受信比赛队伍；已选目标失效、换代或死亡时不能偷偷改成自疗。被拒绝的目标替换保留原弱目标及冻结代次，只有显式清空才撤销意图。生命、冷却和表现继续由服务器确认事件通知已有UI与表现订阅，不增加UI轮询。

## 兼容修复、证据和限制

导入后的旧模块包装需重建。兼容包装保留原始HLSL、逐发射器常量、参数链接、随机动态输入、数组接口及RotationOut输出；传送模块仅补齐Niagara词法需要的位运算空白。旧节点在存在Monolith多链接删除风险时保留禁用。全部42系统已保存、重载、真实编译，0错误。19条历史SpriteSize/RibbonWidth初始化提示单独保留，未宣称全部效果完成视觉验收或性能预算。

原花瓣已通过真实PIE模拟、9层非空粒子/Alpha及FinalColorLDR渲染取证。完整源文件哈希和目标依赖审查通过：不残留旧插件挂载或旧Runtime脚本引用。原生资产由引擎生成、保存；普通文件工具只维护源码、配置与审核材料。

证据目录：`Saved/Validation/PetalBloomIntegration20261015`，包含 `Plan.json`、`Verification.json`、`CompileReloaded.json`、`PetalFrame/Result.json`及真实PNG。客户端Cook为 `6a95a6eb-bd60-485a-b87a-8444d3570146`，治疗/动作/冰箭52项最终容器门禁通过；专服Cook为 `20b718c5-252e-487b-afcd-8609d8286d94`，实际Stage/Pak表现剥离通过。最终构建、定向回归与双客户端进程身份另见同目录 `Runtime.json`，不将进程启动视为人工联机治疗验收。

回退使用本次 `HealingAssetsBefore`、`LibrarySystemsBefore`和模块原始输入证据恢复自有改动；不覆盖其他未提交修改，不改D盘源库。源资源按用户指定本机项目来源记录，未附独立再分发许可材料；本次不执行外部分发。中文说明审核覆盖迁移桥、三个迁移/修复工具、PIE采样、治疗作者与定向回归入口，未声称完成存量全项目注释审计。

2026-10-16增量：19项真实写入、GPU渲染及地面冰箭复核见[VFX初始化与冰箭地面起手](VFXInitializationAndIceCast20261016.md)。原始19条工具提示保留，不能与项目未解决项0混称。


2026-10-18治疗重导：正式治疗已改用独立HealingBloom闭包及三档质量，历史Library继续作为素材预览；新导入、冷加载、受控像素对照与性能边界见[本次交付](HealingVFXReimport_20261018.md)。旧Cook不能作为本次发布证明。
