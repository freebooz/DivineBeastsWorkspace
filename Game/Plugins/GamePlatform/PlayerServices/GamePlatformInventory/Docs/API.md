# API（接口）

客户端模块公开 `UGamePlatformInventoryClientSubsystem（背包客户端子系统）`，提供 Snapshot（快照）、Revision（修订号）、排序、ViewModel（显示模型）、Refresh（刷新）、Retry（重试）、Move（移动/交换）、Split（拆分）、Merge（合并）、Set/Clear Quickbar（设置/清空快捷栏）。

`IGamePlatformInventoryClientTransport（背包客户端传输接口）`只定义异步 Gateway 操作和 `CancelAllRequests（取消全部请求）`；默认实现 `FGamePlatformInventoryGatewayHttpTransport（背包网关HTTP传输）`只访问 `/v1/inventory...`公共接口，不接受 PlayerId（玩家编号），不接触 PlayerData 内部令牌。

Shared Contract（共享契约）包括 `inventory-gateway.v1.yaml（客户端网关背包接口）`与 `playerdata-inventory.v1.yaml（PlayerData内部背包接口）`。Grant/Consume（授予/消耗）不出现在普通客户端公共写接口。

当前没有 Generated（生成代码）修改；协议绑定生成器尚未执行，因此生成绑定状态为未执行。
