# HitValidation（命中验证）

`FGamePlatformCombatHitValidator`实现服务器当前时刻 LineTrace（射线）和 SphereSweep（球形扫掠）。Source 必须有 Authority，ExpectedTarget 必须在同一 World。

配置限制 MaxHitDistance、MaxTraceOriginOffset、MaxSweepRadius；客户端不能提交任意半径或将自己的 HitResult 直接作为权威事实。

LineTrace 首个阻挡 Actor 不是 ExpectedTarget 时返回 Obstructed；超距离返回 OutOfRange；无命中/非法起点等返回 HitValidationFailed。

当前没有 Server Rewind/Lag Compensation。高延迟竞技命中仍基于服务器当前时刻世界状态，这是已知限制，不以其它描述暗示支持回溯。
