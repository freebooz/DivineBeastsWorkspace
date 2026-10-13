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

## 验证状态与欠账

所有未实际执行的验证均标记「待验证」，特别是：
1. Python 启动器纯函数与 YAML 语义测试。
2. UE5.8 Server Target 原生编译（避免把任意静态门禁冒充编译）。
3. Linux Server 包、地图最小 Cook、UE 进程启动与 PID1 信号。
4. Kubernetes/Agones 实际 Sidecar Health → Go Ready → SDK Ready → GameServerAllocation → Drain → Shutdown。
5. OpenWorld/Village 玩家容量 Counter Beta 开关、长生命周期安全缩容、Go 迁移/票据回收。
6. 实际固定 P95/P99 指标、CPU/内存预算与并发五种竞技模式；现有 YAML 数值仅是待批准示例。

## 风险与决定

- 不额外创建 GamePlatformServerAgones 空插件。遵守现有固定代码插件身份：平台内部可选适配、项目组合层下注入、Go 分配权威。
- **绝不因 Fleet Counter 数量减少直接销毁含活跃玩家的 OpenWorld/Village 实例**。必须在端到端验证建立与现有 Go RegistryAllocator 一致的租约/迁移/排空门禁，或在生产保持 Counter autoscaler 禁用。
- Agones Ready REST 调用成功不代表异步 GameServer 状态已变更，需要真实 CRD Watch 验收；项目不可静默发布生产。
