# Authentication（认证）

认证完全复用 GamePlatformOnline。Flow 调用 TryAutoLogin、LoginWithCredentials、Logout，并只观察 FGamePlatformAuthSnapshot（认证快照）。

项目错误模型保留 InvalidCredentials、AccountLocked、Maintenance、NetworkUnavailable、AuthExpired、ContractIncompatible、Cancelled、TimedOut 等可展示错误，不把所有认证失败压成同一个错误。

密码只在调用时传给 Online Provider；Token 只通过临时 Authorization Header 获取，不进入 ViewState、Telemetry 或日志。
