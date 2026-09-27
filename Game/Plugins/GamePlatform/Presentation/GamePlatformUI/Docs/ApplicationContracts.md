# ApplicationContracts（应用契约模式）

平台 UI 只定义 Query Source / Client Model / Command Port（查询源/客户端模型/命令端口）的使用模式，不定义任何游戏业务命令。

标准数据流：Authority Owner（权威所有者）→ Client Model/View State（客户端模型/视图状态）→ ViewModel → Widget；用户操作反向只产生 Intent/Command（意图/命令），由上层适配器执行。

平台层不得直接 HTTP、不得引用 Backend DTO（后端数据传输对象）、不得修改 Gameplay 权威变量。

`DivineBeastsUI（神兽联盟项目UI）`后续应在项目层定义具体 UI-facing ports（面向UI端口），由 ApplicationFlow/Arena/World 等 Owner 的适配器注册实现。
