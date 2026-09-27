# BudgetingCulling（预算与裁剪）

平台预算分两层：第一层为Niagara Effect Type原生Scalability/Culling；第二层为GamePlatformVFX的紧急活动实例上限，防止异常内容无限生成。

Critical表现当前可绕过紧急总量上限，但这不是“所有Local Player VFX永不裁剪”。重要命中确认、关键Ability Cue应通过Effect Type和产品策略评估bAllowCullingForLocalPlayers，而不是全局关闭裁剪。

Diagnostics应记录active、drops、culls、peak和EffectType维度数据；最终阈值必须依据1v1、5v5、OpenWorld、Village、Android实测调整。