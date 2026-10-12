# Task 5-PCG：平台Gold地图创作路径参数化

## 项目与范围
固定Cwebcodex项目 agent:freebooz-4b15fc0c7cf14418:divinebeastsworkspace-888d2615，源根E:\poject\feebooz\DivineBeastsWorkspace，UE5.8根F:\UnrealEngine-5.8.0-release。用户已授权计划后直接执行，不提交/推送，不派生子代理、不停编辑器、不启动UE构建/运行。
先读AGENTS及相关规范。独占本子任务所需的GamePlatformPCGGoldMapAuthoring.cpp/.h、GamePlatformPCGGoldAssetsCommandlet.cpp、Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1及必要的同目录入口/相关测试/文档。Root处理共享总体目录规划与主执行台账。

## 并发保护
根代理发现已有PCG任务正在该文件工作，初始未提交内容包括GoldMapAuthoring、Actors、EditorLibrary、RunGoldSpatialProbe、RunGoldDeterminism与VerifyPCGGold*测试。根已向其Session wc_sess_bO4p9wHESb74ttIp发协调消息wc_msg_TYu4pC9yxK2897jN，尚无回复。不能覆盖或重写整个共享文件，不能移除其空间/生成/时间戳/并发进程/成功日志门禁；先把本子任务将接触的当前文件字节、SHA256与git差异保存到Saved/Validation/PluginArchitectureOptimization/20261012/BeforePCG，再用fresh read_revision的结构化精确编辑。若版本冲突，重新读并保留对方改动；无法安全合并则报告具体冲突，不强改。纯读取和该精确增量已获本任务授权，无需等待用户再次确认。

## 已确认问题与最佳最小改动
平台GoldMapAuthoring.cpp行39硬编码/DBAWorldPack_Village/PCG/Blueprints/，只有Preflight实际组装ClassPath使用。Create/Repair调用Preflight；Verify/SpatialProbe/GeneratePreview可能不需要该根，按实际代码保持最小接口范围。中立GoldRoot与MapPackage仍可保留原/Game/Development/Foundation/PCG身份。
把项目蓝图根作为显式创作输入传入平台Preflight；平台无项目默认值。Commandlet在确实依赖BlueprintRoot的Stage解析并校验-BlueprintRoot=；项目Tools入口提供原/DBAWorldPack_Village/PCG/Blueprints/默认值。优先复用已有FoundationTemplatesCommandlet的BlueprintRoot命名，不新增插件、Settings UObject或公共API。错误/缺参必须在写资产前返回清楚失败；目录规范化和包路径合法性验证复用实际UE5.8 FPackageName接口。保持当前11项中立领域/形状清单、稳定Gold资源路径及不覆盖策略。

## 验证要求
增补生产路径规范化/缺失/非法路径回归，必要时更新既有架构入口检查的调用合同，不能放宽其他断言。轻量Python/PowerShell可执行；已准备PowerShell7.6.6于Saved/Tools/PowerShell-7.6.6/pwsh.exe。UE统一由Root执行。必须证明平台源码中无DBA项目挂载点，项目工具仍提供原默认根，已有PCG未提交改动保留。

## 报告
写Docs/Implementation/PluginArchitectureOptimization/task-5-pcg-report.md，列全部变更/新文件、原始快照路径、接口与兼容行为、完整测试名和真实命令/结果、待UE检查项和未解决风险。聊天只回DONE/具体状态与报告路径。
