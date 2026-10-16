# ScalabilityEffectTypes（伸缩与效果类型）

常规VFX伸缩优先使用UNiagaraEffectType（Niagara效果类型），Definition只保存EffectType引用和bEnableScalability桥接配置。Editor Validator在启用伸缩但未关联EffectType时报告错误。

距离Cull、Effect Type最大实例、Per-System最大实例、Visibility、Significance、Budget Scaling、Platform Set、Cull Reaction和Update Frequency由Niagara原生机制负责。

平台FGamePlatformVFXScalabilityPolicy只保留MaxActiveInstances紧急总量上限与Importance桥接，不每Tick复制所有Cull规则。

## V2.0 三维预算与原生资产门禁（2026-10-16）

- 请求预算（MaxActiveInstances=256、HardMaxTrackedInstances=512）与真实Niagara System数不同；后者必须包含独立组件、NDC Reader、环境及其他入口，不能用`stat GamePlatformVFX`请求数代替。
- 七类候选Niagara Effect Type见`Tools/Unreal/VFX/Arena/ArenaVFXAssetsSpec_V1.json`；Max Effect Type Instances按同类合计，Max System Instances按单个系统资产；120/80/58/40（Epic/High/Medium/Low）仅为待测World总目标，不是默认引擎硬限。
- 按危险程度优先决定核心保留，再对装饰设置距离/可见性/Bounds、预算伸缩和质量变体；不得因为距离远误删真正AOE威胁。GPU/CPU毫秒及Overdraw必须用打包客户端测量。
- 只有真实UE资产经创建、编译、保存、重载并验证绑定后才算Effect Type生效；避免把V2.0 JSON当成原生Niagara配置。
- `UGamePlatformVFXSettings`读取`Config=GamePlatformVFX`类别；平台独立INI默认关闭世界规则。项目层通过`Game/Config/DefaultGamePlatformVFX.ini`追加四档SSLM预算，`Experience.*`前缀仅使用上游真实ExperienceId。配置解析须通过`DivineBeasts.VFX.MainArena.LoadedBudgetSettings`原生自动化验收。
