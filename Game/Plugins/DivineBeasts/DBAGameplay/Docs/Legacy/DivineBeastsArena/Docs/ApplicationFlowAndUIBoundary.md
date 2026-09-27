# ApplicationFlowAndUIBoundary（应用流程与UI边界）

DivineBeastsApplicationFlow核心仍不依赖Arena。

DivineBeastsArenaClient通过IDivineBeastsApplicationFlowExtension注册项目竞技扩展：
- OnEnteredInWorld / OnLeavingInWorld用于观察Flow世界状态。
- PostMatch调用RequestPostMatchReturnToWorld，重新申请OpenWorld Assignment和新Ticket。

本插件不实现正式UI Widget。UI以后只消费GamePlatformArena ViewModel + 项目模式元数据。

客户端不能设置Team、Score、Winner、Phase、Mode或Result。
