# TargetSelection（目标选择）

每个 AI 维护自己的有界 Candidate Map：Actor、EntityId、Generation、LastKnownLocation、LastSensedTime、Visible/Heard/DamageSource；不会永久保存所有曾经见过的 Actor。

资格检查：Actor有效、同World、Provider仍允许、EntityId/Generation匹配、若有 CombatComponent 则未死亡、且不超过 Home Leash。关系/队伍由未来 Provider 注入。

排序由共享纯函数和 Automation Test 共用：Eligible → Visible（配置启用时）→ Distance → LastSensedTime → Stable EntityId；不依赖 TMap 迭代顺序或感知回调顺序。

持续交战为服务器派生策略 `bRetainCombatTarget`，平台默认关闭，SSLM的Village狼/吸血鬼控制器启用。开启后当前有效目标与有实际伤害仇恨的候选不因感知记忆超时或出生点牵引距离被清除；未交战候选仍遵循原有记忆与距离规则。只跳过这两项，跨World、死亡、销毁、Provider拒绝和目标代次变化始终清理。

仇恨取本AI的Combat组件发布的实际伤害 `AppliedMagnitude`，按目标当前实体/生命代次累计；Sight/Damage感知强度和客户端声明不能计分。累计伤害更高者优先，同分维持当前目标，再按既有可见/距离稳定规则排序。底层Combat负责权威事件幂等，候选另拒绝相邻重复事件。总候选数仍受MaxCandidates约束，先保留高仇恨，再保护同分当前目标。离场/死亡/重新占有清空全部候选；不可继承旧化身分数。

持续交战只是选敌与追击，未取消视线、真实导航、技能资格、近战距离和项目新手安全区规则。失去视线仍追击原有效目标，实际攻击只在满足权威资格后执行。UE回归`GamePlatform.AI.Movement.IdlePatrolLifecycle`覆盖记忆超时、靠近不抢仇恨、实际伤害累计转移、重复通知和目标代次/销毁清理；原生运行、Cook与双客户端观察分别留证。
