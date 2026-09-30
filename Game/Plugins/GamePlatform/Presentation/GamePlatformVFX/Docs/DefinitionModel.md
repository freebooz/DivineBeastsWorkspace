# DefinitionModel（VFX定义模型）

`UGamePlatformVFXDefinition` 统一继承 `UGamePlatformDefinitionBase`。身份、结构版本、内容修订和必需 Definition 依赖全部复用 GamePlatformData：

```text
LogicalId
DataVersion
RequiredDefinitions
```

VFX 自身只增加执行字段：Behavior、ContentCategory、NiagaraSystem、EffectType、ParameterSchema/Defaults、Platform/Quality Variants、Fallback、PreloadAssets、Pooling/Scalability、LWC/FixedBounds/Lifetime。

NiagaraSystem、EffectType、平台/质量变体与 PreloadAssets 均进入 `VFXRuntime` Asset Bundle，由 Data World Lease 持有。

`ResolveNiagaraSystem` 按 Platform → Quality → Default 选择表现资产，不改变 Gameplay 尺寸或时序。`ValidateDefinition()` 先执行平台统一 Definition 校验，再执行 VFX 领域检查。

Definition 禁止保存 Damage/Heal/Shield 等 Gameplay 权威结算。