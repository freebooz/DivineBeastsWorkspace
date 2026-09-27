# API（接口）

核心公共类型：

- FDivineBeastsUIViewState（项目UI统一视图状态）：只读业务投影。
- FDivineBeastsUICommand（项目UI命令）：瞬时Intent（意图），不是持久化对象。
- IDivineBeastsUIQuerySource（UI查询源）：提供当前View State并广播变化。
- IDivineBeastsUICommandPort（UI命令端口）：提交/取消UI Intent。
- UDivineBeastsUIClientSubsystem（项目UI客户端子系统）：契约注册、平台页面定义注册、平台页面打开/关闭代理、Loading Screen同步。
- UDivineBeastsUIViewModel（项目通用视图模型）：事件驱动页面状态与命令入口。
- FDivineBeastsUIScreenCatalog（项目页面清单）：稳定Screen/HUD/Notification ID及平台Definition元数据。
- FDivineBeastsUIRoutingPolicy（项目UI路由策略）：只根据View State推荐页面，不推进业务流程。
- FDivineBeastsUILocalization（项目UI本地化映射）：ErrorCode→FText（错误码→本地化文本）。

Password（密码）只存在于非USTRUCT的瞬时命令字段和调用栈，不作为UPROPERTY、View State或日志字段保存。
