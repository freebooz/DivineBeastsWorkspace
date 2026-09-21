# Backend/tests（后端测试）

本目录只负责Go业务后端自身的跨模块、契约适配和架构边界测试。

仓库级 Client（客户端）↔ Dedicated Server（专用服务器）↔ Backend（后端）的Integration（集成）、Network（网络）、Performance（性能）和E2E（端到端）测试统一放在仓库根 `Tests/`，不得复制到本目录。
