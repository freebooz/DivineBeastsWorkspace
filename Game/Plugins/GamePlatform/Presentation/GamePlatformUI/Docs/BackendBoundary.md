# BackendBoundary（后端边界）

本轮新增 Go 业务后端接口：无；新增 Go 微服务：无。

`GamePlatformUI`不知道 Gateway、PlayerData、Match、GameServerControl 等任何业务服务地址或 DTO。

如果 UI 缺少数据，应由业务 Owner/Application 层扩充 Client Model/View State，再通过中立接口投影给 ViewModel；不能给 UI 单独新建 HTTP API 直连。

静态门禁会扫描 MatchService、PlayerDataService、GatewayService 等禁用标识。
