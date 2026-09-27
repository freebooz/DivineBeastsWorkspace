# DecisionFramework（决策框架）

第一版唯一主 Brain 为 `Behavior Tree + Blackboard（行为树+黑板）`。AIDefinition 的 BrainType 必须为 BehaviorTree 才能在服务器初始化；`StateTree（状态树）`明确返回 UnsupportedBrain，不创建空资产冒充支持。

Controller通过 GamePlatformData 异步加载 BehaviorTree/Blackboard，调用 UseBlackboard + RunBehaviorTree，并启动有界 Decision Timer。Blackboard记录目标、最后位置、Home、LOS、Dead、CanAttack 和 GoalLocation。

Controller负责权威事件聚合与高层目标状态，BehaviorTree 资产负责使用原生 Move To / Wait / Decorator 等节点消费 Blackboard；自定义 Task 只增加真正有职责的 GAS 攻击请求。

真实 BT/BB `.uasset`尚未创建，因此 BehaviorTree实际启动状态仍是未执行。
