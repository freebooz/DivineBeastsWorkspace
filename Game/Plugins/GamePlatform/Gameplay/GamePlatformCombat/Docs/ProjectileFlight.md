# 通用投射物飞行

唯一实现位于现有AGamePlatformCombatProjectile及FGamePlatformCombatProjectileMotion。Flight/Profile为可选配置，未启用时兼容原速度字段，但不维护第二个位移执行者。

完整字段、单位、蓝图/C++调用、冻结时机、错误处理、生命周期、网络与验证边界见[通用飞行运动使用与验证](../../../../../../Docs/Implementation/Combat/ProjectileFlight_Usage.md)。所有权威接口仅游戏线程服务器调用；客户端读取中立MotionSnapshot，不决定位置或伤害。模板/曲线发射冻结，非法配置和重复发射拒绝；来源/目标过期、取消、时间/距离耗尽及世界退出均沿既有终态清理。
