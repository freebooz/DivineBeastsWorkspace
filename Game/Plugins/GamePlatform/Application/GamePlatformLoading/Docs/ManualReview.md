# 人工审查清单

AI仅给出建议与风险，不代填人工签名。本表所有人工结论均为未执行、待人工审查；自动化证据见TestingAndEvidence，不等同人工批准。

| 审查项 | 实现位置 | 证据 | 风险 | 结论 | 审查人 | 日期 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 架构与职责 | uplugin、Build.cs、Architecture | 单Runtime静态依赖 | 中 | 未执行 | 待填写 | 待填写 | 建议保持Loading不依赖Session客户端 |
| 代码及DAG/回退 | LoadingPolicy.h | Native Debug/Release 各1/1，46条断言；含任务/依赖容量与Ticker策略 | 中 | 未执行 | 待填写 | 待填写 | 原生策略通过不能替代UE Automation |
| 生命周期与租约 | Subsystem、LoadingBuiltinTasks | U RealLeases未运行 | 高 | 未执行 | 待填写 | 待填写 | 必须核查Ready持有和退出清理 |
| Flow隔离 | DBALoadingFlowNode | 节点身份接线源码 | 高 | 未执行 | 待填写 | 待填写 | 真实取消/重试联调缺证据 |
| 端侧依赖 | 模块依赖与目标 | UE5.8 Editor/Client/Server `-Module=GamePlatformLoading` 均编译通过 | 中 | 未执行 | 待填写 | 待填写 | 编译通过仍不等于Cook/Stage干净 |
| 自动化测试 | Private/Tests、Tests/CMakeLists | 原生46断言通过；UE服务测试源码已编译但未运行 | 高 | 未执行 | 待填写 | 待填写 | 缺可启动UnrealEditor/UnrealEditor-Cmd，按矩阵逐项验证 |
| 手工真实运行 | FoundationLoadingOnly、双PIE | 无运行截图或报告 | 高 | 未执行 | 待填写 | 待填写 | 不得凭日志Success签字 |
| Session真实准入 | 尚缺公开服务适配 | 无 | 高 | 未执行 | 待填写 | 待填写 | 前置阻塞，禁止假Ready |
| Cook产物 | 现有CookFoundation入口 | 本轮未执行 | 高 | 未执行 | 待填写 | 待填写 | 检查Server剥离及Shipping开发资产 |
| 文档一致性 | README+11专题及仓库记录 | 本轮源码与证据路径 | 中 | 未执行 | 待填写 | 待填写 | 建议结论：只接收源码，不批准生产 |
