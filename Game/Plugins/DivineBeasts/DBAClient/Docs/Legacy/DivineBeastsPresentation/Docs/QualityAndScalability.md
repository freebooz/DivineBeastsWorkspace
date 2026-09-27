# QualityAndScalability（质量与伸缩）

FGamePlatformPresentationContext提供PlatformId、QualityTier和LocalPlayerRelation等表现提示。

项目Catalog可以用ContextQuery选择逻辑Definition覆盖，但具体PC/Android、分辨率、特效预算、降低动效、音频质量和动画LOD仍由对应Provider执行。

质量差异只影响表现，不得改变伤害、命中、任务结果、世界分配或Arena规则。

当前没有真实Android/PC表现资产和压力结果，质量伸缩运行验证为“未执行”。
