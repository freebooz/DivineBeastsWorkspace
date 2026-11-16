# 火冰V3技能栏接入

本次沿用用户选择保留的V3资源，将12个角色的按钮1对应火法、按钮2对应冰法。按钮3的定向飞弹及按钮5的治疗保持原有授权。

## 实际变更

- 复用项目适配层现有双掌生命周期，使火法和冰法从真实左右手骨、指根构造掌面挂点，消费服务器确认的施法窗口；同步取消、迟到就绪和后继施法仍使用原有身份保护。平台VFX唯一执行链、服务器技能时长、消耗、伤害与弹道没有修改。
- 通过Monolith修改、保存并重新加载四个稳定Definition。`DevelopmentMagic/Casting/Definitions/DA_DBA_VFX_DevFireballCast`及`DA_DBA_VFX_DevFrostboltCast`引用`OrbProjectile/Review/V3/PalmRedesign/Niagara`中的`NS_DBA_FirePalmCast_V3_ConceptReview`、`NS_DBA_IcePalmCast_V3_ConceptReview`。
- `DevelopmentMagic/Definitions/DA_DBA_VFX_DevFireballFlight`及`DA_DBA_VFX_DevFrostboltFlight`引用`OrbProjectile/Review/V3/CastFlight/Niagara`中的`NS_DBA_FireOrbFlight_V3`、`NS_DBA_IceCrystalFlight_V3`。以上路径的共同挂载前缀均为`/DBAPresentationPack_Core/VFX/`。
- 掌心Definition增加三个真实float白名单参数：`User.CastDurationSec`（0.001–600）、`User.ElapsedAtStartSec`（0–600）、`User.HandSideSign`（-1–1）。不修改原始公共材质或Niagara系统。
- V3没有独立命中资源，因此沿用现有命中效果；本次没有制作最新概念图中的飞行、命中重设计。

## 验证

- 修改前原生双掌回归失败，修复后21项原生测试全部成功，覆盖火冰双掌、动画窗口、重复通知、同步取消、后继请求、音效生命周期及技能栏投影。测试提供者仅记录请求边界，不能代替实际Niagara视觉验收。
- Editor、Client、Server完整目标均使用既定增量参数编译成功，构建前后源码指纹固定。
- 原生资源读回四个Definition与目标完全一致，编辑器无未保存内容。首次设置只读结构体字段失败后，重载未保存的Definition，改用既有作者工具采用的结构体构造方式成功保存。
- 第一次测试选择器仅运行18项，18项全部成功，但数量门禁正确失败；修正选择方式后重新运行完整21项，未改写失败记录。
- Cook、真实部署和两个登录窗口的最终结果见`Saved/Validation/FireIceV3Binding20261027/DeliverySummary.json`；该文件生成前不表示部署完成。人工登录、施法视听及性能验收仍需单独确认。

所有构建、Cook、Stage、备份与核验证据均在当前工作空间Saved目录内。未提交主分支。
