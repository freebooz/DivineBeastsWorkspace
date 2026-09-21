# GamePlatformLoading 实施进度

2026-09-21；本轮按用户最新附件实施第六插件，不继续扩大 Session 后端范围。

## 现场与设计约束

- main 分支；开始复核时 HEAD 为 695d7cf。工作树存在 Foundation、Online、Session 等并行未提交修改，禁止覆盖。
- Loading 目录为空；Data 的真实入口为 IGamePlatformDataService::Get / AcquireDefinition / ReleaseDefinition，成功租约需要显式释放。
- Flow 有带 ScopeId、RunId、NodeId、NodeGeneration 的节点完成语义；由主工程连接 Loading，不建立反向模块依赖。
- Session 仅有私有原生状态内核，没有公开会话服务或真实准入链。会话加载验收未执行，前置阻塞。Online 正由其他改动补齐，不能视作已联调。
- 引擎为本机 UE5.8.0，Foundation 尚无真实地图/定义生成及目标通过证据。OverallPlan.md 不存在，采用正式中文总体规划。

## 执行步骤

1. 实施并运行生产任务图内核的失败优先测试。
2. 增加单 Runtime 模块、私有实例子系统、公开类型化任务与服务；Data 和世界任务复用真实接口。
3. 主工程 Flow 的 Ready 阶段接入 Loading，取消时解绑并释放；不接假 Session。
4. 执行原生测试和可用的 UE 检查，记录环境阻断，不绕过原有工程门禁。
5. 补齐十二份中文材料、目录登记和分项验证矩阵。

## 所有权决定

每个实例仅一个未释放的操作，包括 Ready 后仍持有资源的操作。Ready 是屏障终态而非资源释放；调用者必须 ReleaseLoadingOperation。失败/取消/超时自动释放任务资源。成功操作释放后才能开始下一次。快照通知延后，工厂在活动操作期间不可撤销；任务图启动后冻结。没有进度插值，也没有自动重试或登录。
