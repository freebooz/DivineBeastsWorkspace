# Backend/tests（后端测试）

本目录只负责Go业务后端自身的跨模块、契约适配和架构边界测试。

仓库级 Client（客户端）↔ Dedicated Server（专用服务器）↔ Backend（后端）的Integration（集成）、Network（网络）、Performance（性能）和E2E（端到端）测试统一放在仓库根 `Tests/`，不得复制到本目录。

`commerce_contract_test.go`只读Shared交易契约，验证写请求不得接受客户端玩家身份及操作ID唯一性。Schema边界通过相邻同级标题截取，兼容Go RE2及Windows/Linux行结束符；禁止使用不受Go支持的前瞻正则而令整个测试进程崩溃。测试通过不证明支付或交易服务已经上线。

`contract_compatibility_test.go`中的操作ID扫描仅匹配现行契约六空格缩进的HTTP操作节点，排除Schema内八空格的同名业务字段。若契约布局改变，必须同步审查此静态门禁；不能通过忽略真实接口重复ID使检查通过。
