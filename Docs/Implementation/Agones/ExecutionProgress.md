# Agones 集成执行记录（2026-10-13）

## 已读取真实工程现状

- GamePlatformServer（平台控制面）已包含 Register / Heartbeat / Ready / Drain / Stop 状态机、串行重试与失效回调。
- DBAServer（SSLM 组合层）按 Deploy/Server/{OpenWorld,Village,MainArena}/server-profile.json 校验资源并向 Go 控制面申请 Ready。
- Backend/internal/platform/agones/allocator.go 已有对 GameServerAllocation CRD 的 Go 原子分配实现；未复制此能力到 UE。
- Runner 之前处于 4 个 detached Job 长期运行，另有 2 个脚本排队；6 个原有任务均收到停止请求，2 个脚本已观察为 stopped，另 4 个仍须查看终态，不得宣称已经全部停止。

## 本轮新增与修改范围

- GamePlatformServer/Source/GamePlatformServer/Private/Agones/GamePlatformAgonesBridgeSubsystem.{h,cpp}：在现有 ServerOnly 插件增加可选 Agones REST Sidecar 生命周期；游戏线程状态、单飞请求、短超时、健康探测、就绪/关闭、权威玩家 Counter；可共享实例出现首个玩家后自标记 Allocated，防止缩容误删活跃服务器。
- GamePlatformServerLifecycleSubsystem.{h,cpp}：新增玩家人数采样委托，用于把既有权威心跳人数传给平台 Agones 适配器，不增加项目层依赖。
- Deploy/Agones/launch_game_server.py：严格解析 SDK GameServer 实际身份与动态端口，并装配单一 UE Dedicated Server 目标。
- Deploy/Agones/fleet-*.yaml：三种角色的示例 Fleet，含动态 UDP 端口、进程健康与资源限制；模板值并非生产实测数据。
- Deploy/Agones/autoscalers.yaml：MainArena Buffer、常驻世界和新手村 Counter 扩缩样例。
- Deploy/Agones/allocator-rbac.yaml：Go 原子分配的最小 Namespace 级 RBAC。
- Deploy/Agones/README.md：依赖、配置、三角色与验收门禁。
- Tests/Architecture/TestAgonesDeployment.py：启动器配置安全边界与 YAML 结构合同。

## 本次续建与修复（2026-10-13）

- 已增量修复 `GamePlatformAgonesBridgeSubsystem.{h,cpp}` 的 SDK 调度优先级：此前配置 `AGONES_HEALTH_INTERVAL_SECONDS=1` 时，Health 容易重复占满所有空闲调度，导致 Ready/Allocate/Counter 饥饿。改为 Shutdown 最高、Health 超期 2 秒抢占、其他时期优先推进 Ready/Allocate/Counter；仍保持每实例单飞 HTTP 和超时边界。
- 新增 `GamePlatformAgonesBridgeSchedulingTests.cpp`（平台层原生调度测试）：覆盖健康和 Ready 同时到期、健康逾期、实例保护、人数 Counter、Shutdown 终态以及 Draining 拒绝重新 Ready。
- 加固 `Deploy/Agones/launch_game_server.py` 的身份校验：拒绝非法 Kubernetes GameServer 名称、无效 DNS 标签、回环／未指定／组播／链路本地公网端点、非法端口、角色与 Profile 不一致、异常地图包路径。未修改 Go 原子分配及业务准入契约。
- 更新 `Tests/Architecture/TestAgonesDeployment.py`：补充端点和 GameServer 身份拒绝用例、角色 Profile 篡改拒绝用例；该脚本仅覆盖启动器和部署合同，不证明实际 UE/Agones 网络闭环。
- 后续安全补强：启动器访问 `http://127.0.0.1` Sidecar 时显式禁用环境 HTTP(S) 代理与跨地址 HTTP 重定向；验证 SDK 身份/状态/端口字段类型，拒绝非法响应。离线回归加入有效本机 SDK 读取夹具、非对象响应、恶意重定向和代理配置检查。
- 配置一致性门禁：Python YAML 语义测试改为按环境变量名称而非数组序号核对容量，并交叉比对 Fleet `server-role/experience/region/build-version` 标签、三类 Profile、协议版本、机密注入及 MainArena 与世界 Counter 分工。
- 启动器和 Sidecar 的行为已按 Agones REST SDK 官方参考文档核对：`POST /health`、`POST /ready`、`POST /allocate`、`POST /shutdown`、`PATCH /v1beta1/counters/{key}`；最终 SDK 2xx 不是 Kubernetes 状态收敛保证。
- 已提交 Python 启动器/部署合同测试（Job `wc_job_oryy2mnVoQPJLOMX`）及 Go Agones 分配回归（Job `wc_job_uHOQVU9IA_sIUlIg`），Runner 并发槽当前被 4 个既有独立进程占满，两项测试尚待执行并读取终态。不能将排队任务写成已通过。现有其他任务未提交文件不得覆盖、重置或删除。

### Go 分配器续修

- `Backend/internal/platform/agones/allocator.go`（Agones 原子分配适配器）：严格要求唯一命名的 `game` 游戏端口，不再回退其他服务端口；端口范围为 1～65535；公网连接端点用 `net.JoinHostPort` 正确处理 IPv6 方括号，非法 DNS、回环地址、伪数字 IP、控制字符均失败关闭。
- Kubernetes CRD 请求只接受结构受限的 HTTPS 基址，并阻止包含 ServiceAccount Bearer 凭据的 HTTP 重定向；非 2xx 只回传状态码，不回显可能携带敏感内容的返回正文。
- `Backend/internal/platform/agones/allocator_test.go`（Go 回归）新增 IPv4/IPv6、DNS/伪数字地址、端口错选和越界、HTTPS 强制及重定向目标保护测试；Go 测试仍以实际运行结果为准。
- 核查生命周期生产调用发现：`DBAServer` 暴露 `BeginDrain`、`CompleteDrain`，但本轮源码搜索没有找到 MainArena 完成结算、迁移和清理后的 `CompleteDrain` 实际调用方。**该运行闭环仍未完成**，不得把存在 SDK `/shutdown` 实现等同于比赛结束能自动回收实例。

## 2026-10-14 实际测试与编译收据

- **Python 部署合同测试通过**：`python Tests/Architecture/TestAgonesDeployment.py -v`，10 项运行、0 失败、0 跳过。首轮因 Python `build_opener` 不保留空 `ProxyHandler` 的错误测试断言失败，已精准修正并复测退出码 0；PyYAML 存在，Fleet/Autoscaler YAML、角色配置及 RBAC 检查均执行。
- **Go 控制面测试通过**：在 `Backend` 下运行 `go test ./internal/platform/agones ./internal/modules/gameserver ./internal/app/gameservercontrol -count=1`，三个包均返回 `ok`；同范围 `go vet` 退出码 0。这些结果不代替生产集群验证。
- **平台静态架构门禁通过**：`pwsh -NoProfile -NonInteractive -File Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Tests/Scripts/TestServerArchitecture.ps1`，含服务器插件依赖、HTTP 加固、准入生命周期检查；不属于 UE 编译或网络联调。
- **UE5.8 Win64 Development Server 首次构建失败**：锁定源码引擎 `F:\UnrealEngine-5.8.0-release`（5.8.0），执行 `Build/Game/BuildFoundation.ps1 -Server -UsePrecompiled -NoUBA` 时 UBT 退出码 6，原因是源码引擎缺少 `ImageCore.precompiled`，并非已证实的 Agones C++ 编译问题。证据：`Saved/Validation/FoundationM0/b1b387e0-a26f-44ab-9b3b-f526f8c4a6bf/Build/Server`。
- **正确参数的 Server 重编译已发起**：移除 `-UsePrecompiled`，保留 `-NoUBA -MaxParallelActions 2`，目标仍为 `DivineBeastsArenaServer Win64 Development`，UBT 构建计划 1086 个动作。Job：`wc_job_BumbRVjhkpfXfnCS`；日志目录：`Saved/Validation/FoundationM0/994e1860-9f53-4688-9aa1-be71f9c6b77d/Build/Server`。**未收到终态前不得标记编译通过**。
- **工作区保护**：`main` 仍有 1 个无关目录规划文档 Git 合并冲突以及并行资产改动；托管 worktree 创建返回 `managed_worktree_root_unavailable`。本轮只修改了 Agones Python 测试与本记录，未处理冲突、清理或覆盖其他文件。
- **仍未验证**：UE 编辑器原生测试 `GamePlatform.Server.Agones.BridgeScheduling`、当前 Server 编译终态、Linux Server Cook/打包、实际 Agones/Kubernetes Sidecar 状态收敛与自动扩缩。

## 验证状态与欠账

以下是当前尚未完整验收的项目；2026-10-14 已通过的 Python/Go/静态门禁证据以上方实际测试收据为准：
1. Python 启动器纯函数与 YAML 语义测试已通过（10/10），该项静态合同门禁已完成。
2. UE5.8 Server Target 原生编译：移除错误预编译参数后正在重新执行，必须等待实际构建终态，不能将中间进度视为通过。
3. Linux Server 包、地图最小 Cook、UE 进程启动与 PID1 信号。
4. Kubernetes/Agones 实际 Sidecar Health → Go Ready → SDK Ready → GameServerAllocation → Drain → Shutdown。
5. OpenWorld/Village 玩家容量 Counter Beta 开关、长生命周期安全缩容、Go 迁移/票据回收。
6. 实际固定 P95/P99 指标、CPU/内存预算与并发五种竞技模式；现有 YAML 数值仅是待批准示例。

## 风险与决定

- 不额外创建 GamePlatformServerAgones 空插件。遵守现有固定代码插件身份：平台内部可选适配、项目组合层下注入、Go 分配权威。
- **绝不因 Fleet Counter 数量减少直接销毁含活跃玩家的 OpenWorld/Village 实例**。必须在端到端验证建立与现有 Go RegistryAllocator 一致的租约/迁移/排空门禁，或在生产保持 Counter autoscaler 禁用。
- Agones Ready REST 调用成功不代表异步 GameServer 状态已变更，需要真实 CRD Watch 验收；项目不可静默发布生产。
