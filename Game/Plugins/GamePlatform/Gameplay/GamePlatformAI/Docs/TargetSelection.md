# TargetSelection（目标选择）

每个 AI 维护自己的有界 Candidate Map：Actor、EntityId、Generation、LastKnownLocation、LastSensedTime、Visible/Heard/DamageSource；不会永久保存所有曾经见过的 Actor。

资格检查：Actor有效、同World、Provider仍允许、EntityId/Generation匹配、若有 CombatComponent 则未死亡、且不超过 Home Leash。关系/队伍由未来 Provider 注入。

排序由共享纯函数和 Automation Test 共用：Eligible → Visible（配置启用时）→ Distance → LastSensedTime → Stable EntityId；不依赖 TMap 迭代顺序或感知回调顺序。

Threat System（威胁系统）：未实现。第一版没有用永远为0的 ThreatScore 伪装仇恨系统。
