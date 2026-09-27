# BootstrapAndInitialization（启动与初始化）

UDivineBeastsApplicationFlowSubsystem（项目流程协调子系统）是 UGameInstanceSubsystem，因此跨地图保持生命周期。

Initialize 获取平台 Flow、Online、Session、Loading 子系统，注册项目节点和状态监听，并创建项目 Backend Adapter。StartFlow 会取消旧HTTP、Loading、Session transfer，使旧 Platform Flow 失效，再启动新的 Boot→Initialize→Authentication 流程。

不使用 GWorld、地图Actor或进程级 static 保存当前流程。
