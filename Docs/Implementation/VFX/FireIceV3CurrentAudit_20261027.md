# 火冰 V3 当前实现与接入审查

审查时间：2026-10-27。直接连接 UE5.8.0 原工程 DivineBeastsArena，编辑器进程 63856。此次检查没有写入特效、地图及正式定义的磁盘资产；同步编译使六个审核系统产生内存脏标记，不将该派生状态保存到源包。版本检查确认原生资产没有磁盘修改。

## 结论

V3 已有真实原生审核资源，但没有完成整套游戏接入与视觉验收。不能说 V3 完全没有实现，也不能说整套已经完成。

| 范围 | 当前状态 |
| --- | --- |
| 原 V3 火掌、冰掌、火球飞行、冰晶飞行 | 四份真实 Niagara，位于 OrbProjectile/Review/V3/CastFlight；本次同步编译均为零错误、零警告 |
| 后续重设计的火掌、冰掌 | 两份真实 Niagara，位于 OrbProjectile/Review/V3/PalmRedesign；本次同步编译均为零错误、零警告。已有保存、独立进程重载和运行证据 |
| 最近设计图的火球／冰晶飞行 | 目前仍是概念图和实施计划；资产注册表没有 FlightRedesign 目录 |
| 最近设计图的火球／冰晶命中 | 目前仍是概念图和实施计划；资产注册表没有 ImpactRedesign 目录 |
| 正式运行引用 | 六个施法、飞行、命中定义全部仍指向旧系统，且没有质量或平台变体切换到 V3 |
| 人工视觉及性能验收 | 未完成。编译成功不能代替视觉、过绘、多人实测和人工签收 |

## 当前正式引用（原生读回）

定义公共前缀：`/DBAPresentationPack_Core/VFX/DevelopmentMagic`。Cast 定义在 `Casting/Definitions`；Flight、Impact 在 `Definitions`。

| 定义后缀 | 实际 Niagara 路径 |
| --- | --- |
| DA_DBA_VFX_DevFireballCast | /DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Niagara/NS_DBA_DevFireballCast |
| DA_DBA_VFX_DevFireballFlight | /DBAPresentationPack_Core/VFX/DevelopmentMagic/Niagara/NS_DBA_DevFireballFlight |
| DA_DBA_VFX_DevFireballImpact | /DBAPresentationPack_Core/VFX/DevelopmentMagic/Niagara/NS_DBA_DevFireballImpact |
| DA_DBA_VFX_DevFrostboltCast | /DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_Cast |
| DA_DBA_VFX_DevFrostboltFlight | /DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_Flight |
| DA_DBA_VFX_DevFrostboltImpact | /DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_Impact |

项目适配仍将服务器事实解析到 `presentation.dba.development.magic.<fireball/frostbolt>.<cast/flight/impact>@1`，不会因为 Review 目录出现 V3 资源便自动使用。此前技能栏映射修改不等于特效定义引用已更新；这一遗漏解释了“按钮修改后仍看到旧VFX”。

## 已知视觉问题与复用边界

2026-10-26 的真实运行记录指出：原火掌分散且条带感强；原冰掌在固定曝光下几乎不可见；火球尾迹过细；冰晶偏石块感。后续掌心审核版已经改善，但仍未通过概念图一致性及过绘门禁。冰材质编译指令较多；该统计不能换算为实际 GPU 耗时。

保留并复用原生资源、材质和模块，在现有审核目录演进；补齐最新飞行和命中设计。用户已授权将完整新版用于按钮 1 火法、按钮 2 冰法的开发验证；这不是人工视觉批准或生产发布许可。继续沿用唯一平台 VFX 执行链，不新建框架，不修改服务器技能权威。

## 证据

- 本次实际工程、六个定义引用和资产注册表：`Saved/Validation/FireIceV3CurrentAudit20261027/NativeReferences.json`。
- 本次六系统同步编译诊断：同目录 `NativeDiagnostics.json`。
- 原 V3 运行记录：`Docs/Implementation/VFX/FireIceV3_VisualInspection_20261026.md`。
- 后续掌心保存、重载、运行、过绘与性能边界：`Docs/Implementation/VFX/FireIcePalmsNativeReview_20261026.md`。

本次没有重新拍摄游戏运行画面，不把历史截图或编译结果写为此次联网验收。
