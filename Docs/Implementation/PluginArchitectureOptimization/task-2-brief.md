# Task 2：GamePlatformData唯一性索引
# 共同约束
- 用户已授权“给出实施计划最佳方案，然后按计划执行”；本轮在固定项目原工作区增量修改，不提交、不推送、不删除工作区。
- 项目：agent:freebooz-4b15fc0c7cf14418:divinebeastsworkspace-888d2615；实际根目录 E:\poject\feebooz\DivineBeastsWorkspace。通过 Cwebcodex 读取/修改，先读 AGENTS.md 与所属规范。不得把本地 scratch 当成项目。
- 保留现有47个代码插件、17个内容包、三层边界和稳定插件/模块/反射/资源身份；不增设第二工程或 DevHost。
- 开始时有其他 PCG 任务的10个未提交文件；本任务无权覆盖、还原或停用其他任务。只修改本 brief 所属文件，其他问题写报告。
- 源码采用 canonical edit_project_files 结构化编辑；现有文件须带 fresh read_revision。direct 不可用时按 tool_manifest 使用 call_runtime_tool。
- 不修改 Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md 或共享执行台账，由主代理统一登记。
- 不并行启动 UE 构建/Cook/Editor，不关闭已运行编辑器。主代理统一执行昂贵验证；可独立执行有意义的轻量回归。
- 中文职责、线程、取消与生命周期注释；声明代码验证、Automation、Cook/Stage、视觉/联机证据各自边界，不虚报通过。
- 不派生子代理；完成后写对应 task-N-report.md，列文件/接口/关键设计、实际测试命令与输出、待主代理验证、疑虑。聊天仅返回状态与报告路径。

## 独占范围
Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData 下的 Private/Validation、相关模块生命周期、必要的 Private/Tests 以及该插件README/Docs。避免修改公开加载/租约签名。需要其他目录请先报告。
## 已确认问题
ResolveUniqueGamePlatformDefinitionSource 每次调用 Registry.GetAssetsByClass(UGamePlatformPrimaryDataAsset,...,true)，而AssetManager.AddDemand在缓存前即调用，依赖节点/SFX多次请求引发重复全扫描。既有Manager外部LoadPrimaryAssets基线是有意兼容机制，必须保留。
## 目标设计
LogicalId→唯一源路径集合的模块私有索引；冷建仅在AssetRegistry发现完成后。监听资产added/removed/updated/renamed和发现完成，更新或明确失效后延迟重建；重复普通查询不能重扫全库。须在GameThread，显式解除委托和清理，不能持有世界/PIE对象。先确认UE5.8现有事件与资产类判断，避免把非定义资产LogicalId索引进去。游戏运行Cook资产发现正确；启动扫描中仍返回AssetRegistryNotReady。
## 行为兼容
缺失→MissingDefinition；重复→DuplicateLogicalId；EditorCandidate替换自身注册路径后参与唯一性；非法ID照旧；UniqueSource与AssetManager映射不一致仍失败。资产变化后不得返回旧ID源，重复解除应恢复可用。稳定PrimaryAsset和public API不变。
## 验证
新增独立生产索引行为测试及实际AssetRegistry编辑器回归（复用既有测试架构/临时资产清理）；覆盖cold/warm/repeated查询、增删改名/LogicalId变化、重复修复、编辑候选替换和取消/Shutdown。用可测查询/重建计数说明避免重复全扫描，不预编虚构性能百分比。保持现有Data lease/recursive/PIE测试。昂贵UE构建/Automation由主代理运行，报告必须列新增Automation完整测试名称及所需参数。写 task-2-report.md。
