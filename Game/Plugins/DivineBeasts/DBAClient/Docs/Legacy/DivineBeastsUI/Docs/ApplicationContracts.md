# ApplicationContracts（应用契约）

UI-facing（面向UI）契约位于 DivineBeastsUIClient：

- IDivineBeastsUIQuerySource（UI查询源）
- IDivineBeastsUICommandPort（UI命令端口）
- FDivineBeastsUIViewState（UI视图状态）
- FDivineBeastsUICommand（UI命令）
- FDivineBeastsUICommandResult（命令提交结果）

DivineBeastsUIClient 不引用 UDivineBeastsApplicationFlowSubsystem（项目应用流程子系统）或 UDivineBeastsArenaClientSubsystem（项目竞技客户端子系统）。

UDBAUICompositionSubsystem（UI组合根适配器）在DBAClient中实现Query/Command接口，将业务Owner公开状态映射给UI，并把UI Intent转回Owner。

命令必须携带RequestId、PageGeneration和ExpectedRevision。组合根发现View Revision已变更时返回 UI.Command.StaleView（陈旧视图），防止旧页面修改新业务状态。
