# ClientFlowAndUIBoundary（客户端流程与界面边界）

`GamePlatformArenaClient（竞技客户端模块）` 提供Matchmaking Adapter（匹配适配）、转服/连接状态、竞技请求模型和只读ViewModel（视图模型）。

UI不在Arena事实模块内实现。已有GamePlatformUIClient（游戏平台界面客户端）时由其读取ViewModel；没有稳定UI接口时也不反向建立循环依赖。