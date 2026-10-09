# F27 已验证出生变换（2026-09-30）

平台Gameplay GameMode持有服务器出生策略和权威生命周期；Source是出生候选身份/保留占用来源，Candidate.Transform是已经校验的实际生成变换。受保护内部出生操作经RestartAtValidatedTransform将Candidate.Transform传入Super::RestartPlayerAtTransform，避免偏移策略验证安全点后却按Source Actor原位置生成。外部直接绕过受保护操作仍由既有资格门禁拒绝。

没有变更SpawnCandidate发布字段、角色枚举或服务器角色。游戏线程权威调用，失败仍沿用既有出生、初始化、准入清理流程。Source必须继续满足候选来源与Reservation合同，不因实际Transform独立就放宽出生权限。

涉及 Private/Framework/GamePlatformGameModeBase.cpp、Private/Policies/SpawnCandidateOperation.h及Private/Tests/SpawnCandidateOperationTests.cpp；纯值fixture故意令Source位置10、Candidate位置110，核对真正传给最后出生调用的110。该测试证明操作选择，不代替UE碰撞、Possess、初始化和联机出生运行。

## 验证与中文审核边界

本次修改已补上述责任、端侧、游戏线程、所有权、失败和取消合同；新增C++测试放在模块Private/Tests，插件Tests只持原生编译入口。原生结果与UE Automation/Editor/Client/Server编译分别记录于Game/Saved/Reviews/task3-repair-report.md，不能将纯规则通过当引擎时序通过。存量公开类型仍有逐字段中文说明缺口，本页不宣称全插件或全工作空间已经整体合规。
