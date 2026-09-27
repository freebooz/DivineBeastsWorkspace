# ManualReview（人工审查）

状态：待人工审查。

当前没有真实 UE5.8 Development GameplayEffect/GameplayCue `.uasset`、中立网络测试地图、Dedicated Server + 双客户端运行证据，也没有延迟/丢包场景日志。AI 不代签人工审查。

人工审查应至少覆盖：服务器 Damage/Shield/Healing、Stun/Silence 到期与移除、单次 Death、Dead Healing 拒绝、Respawn Reset、晚加入、Avatar Replacement、Unauthorized 请求、延迟下无双扣血，以及客户端/服务器 Cook 资产边界。

Root 与 Lag Compensation 本轮明确不在已实现范围，审查时不得按“失败”误解为回归；它们属于未实现/不适用的后续能力。
