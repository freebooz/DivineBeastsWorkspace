# SSLM气势系统P0整改及验证记录（2026-10-14）

## 规则决策
采用2026-10-09后的正式精简工程基线：出生气势0、上限100，保留两项气势GAS属性。旧阴阳潮、脱战回归50、五行/破元/共鸣未获当前工程规则批准，本轮不恢复。增加`FDivineBeastsMomentumDefinition.BasicAttackHitGain`（普通攻击命中收益），默认10点为开发占位值、非正式策划批准；12个生肖英雄可通过各自HeroDefinition（英雄定义）单独调整。

## 服务器权威链路
1. `DivineBeastsPlaceholderGameplayAbility`（占位技能执行器）仅在Primary（普攻）成功Commit（GAS技能正式提交）、真实射线命中、平台`GamePlatformCombat`（战斗系统）实际扣血或耗盾后将真实`FGamePlatformCombatResult`（结算结果）交给本角色。挥空、受拒绝和无伤害不奖励。
2. `UDivineBeastsCharacterComponent::AuthorityAwardMomentumForConfirmedBasicHit`（可信增势）校验服务器权威、当前世界、HeroDefinition成功加载、IsCharacterReady（角色就绪）、AvatarGeneration（角色代次）、同一ASC（技能组件）、有效EventId（事件标识）、非零真实伤害；按`min(BasicAttackHitGain, MaxMomentum-Momentum)`计算，单身份最多保留128个已奖励事件ID，重绑/退出清空。
3. `UDivineBeastsMomentumGainGameplayEffect`（项目原生即时气势效果）通过稳定GAS SetByCaller名称`SSLM.Momentum.BasicHitGain`配置正值并修改唯一`Momentum`属性，不额外创建永久数值或跨层插件。
4. 主动法术Cost GameplayEffect（技能消耗效果）与Cooldown（冷却）沿用现有服务器GAS：火焰弹10、寒冰箭8、奥术飞弹12为临时占位成本。项目技能栏只读取GAS并在资源不足时显示「气势不足」。
5. `ValidateCompactGASAttributes.py`（属性门禁）修正唯一ASC已搬至Pawn基类后的误报；`ValidateZodiacAbilityIntegration.py`（技能门禁）添加P0调用、鉴权、效果与UI只读合同断言。

## 资产兼容及验证限制
- 2026-10-14实施前UE5.8原生只读核对12份英雄资产：InitialMomentum（初始气势）均0、MaxMomentum（上限）均100；12套AbilitySet（技能集合）Effects（启动效果）均空。新字段不直接覆盖二进制英雄资产，需编译后真实UE只读检查默认10迁移是否生效。
- 代码回归：`DivineBeasts.Characters.MomentumContract`（配置边界）和`DivineBeasts.Characters.Momentum.RealGameplayEffect`（真实GE应用）。务必保留Editor/Client/Server构建退出码、UE自动化结果和新手村服务器双客户端测试证据，禁止以脚本PASS冒充全部验收。
- 新手村真正可玩验收必须覆盖：出生0、挥空0、成功普攻+10、重复EventId零重复奖励、10点释放法术并扣费、耗尽拒绝、满100不溢出、换角色与重生清账及OwnerOnly气势复制。其他历史已退役机制未实施。
