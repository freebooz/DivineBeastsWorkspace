# SSLM Agones（专用服务器容器编排与自动扩缩）集成

## 1. 已有架构与适用边界

本目录属于 Deploy（部署层），不新增违反稳定插件清单的空插件。三层关系为：
- GamePlatformServer（平台层）的 Private/Agones 子系统：调用 Agones Sidecar REST SDK 的 Health（健康）、Ready（就绪）、Shutdown（销毁）、Beta Counter（容量计数）；可共享实例出现首个玩家时用本地 Allocate（防止缩容删除活跃世界），仅在 Dedicated 且注入 AGONES_SDK_HTTP_PORT 时启用。
- GamePlatformArena（MOBA通用层）：既有竞技规则与服务器，不需要直连 Kubernetes。
- DBAServer（SSLM项目层）：OpenWorld、Village、MainArena 统一启动目标；校验 Profile、真实世界、角色资源与 Go 控制面状态，不复制 SDK 网络实现。
- Backend（Go控制面）：现有 agones/allocator.go 通过 Kubernetes GameServerAllocation 原子分配 MainArena。世界和新手村沿用 RegistryAllocator 的多玩家/多实例分配，不能误用一次分配即独占整个常驻世界。

根工程规则禁止恢复 GamePlatformOpenWorld 等旧空插件；因此本轮将 Agones 作为 GamePlatformServer 的 **可选内部 Sidecar 适配能力**，不建立多余的平台插件身份。

## 2. 三类 Fleet（服务器舰队）

- sslm-main-arena：每场比赛占用一个 MainArena，支持 1v1 至 5v5，Buffer（空闲实例缓冲）自动扩缩。示例保留 2 个 Ready 实例，最大 30；需求由 Go 通过 GameServerAllocation 提交，不由游戏进程自行 Allocate。
- sslm-open-world：OpenWorld 包含大厅、主城、开放世界。Counter（剩余玩家容量）自动扩容，示例单服容量 40、空闲容量缓冲 20。
- sslm-village：新手村包含教学与训练，Counter 自动扩容，示例单服容量 20、空闲容量缓冲 10。

**缩容安全门禁：** Counter 是 Agones Beta 功能，不代表其能判断 Go 内部的已发放但尚未连接的票据、玩家迁移窗口或持久世界存档事务。当前 Counter Fleet 配置只是可供联调的扩缩模板；生产开启自动缩容前，还必须建立：停止分配 → 控制面停止准入 → 玩家有序迁移或退出 → 存档及事件提交 → 本地玩家数归零 → CompleteDrain → Sidecar Shutdown。未通过上述端到端验证时不能自动删除活跃常驻世界实例。Fleet 的 eviction.safe=Never 主要保护节点驱逐，不等于 Fleet 缩容保护。

## 3. Sidecar SDK 和状态次序

1. Agones 启动 Pod 并注入 AGONES_SDK_HTTP_PORT。
2. launch_game_server.py 访问本机 SDK GET /gameserver，提取实际 GameServerName、动态 UDP 端口与节点主地址；超时或字段错误不启动游戏，不伪造固定端口。
3. 使用同一个 DivineBeastsArenaServer Linux 产物与 Deploy/Server/对应角色 Profile 启动。
4. UE GamePlatformServer 常规注册 Go 控制面；DBAServer 校验世界/必需资源并发布业务 Ready。
5. 只在平台 Lifecycle 状态 Ready 后，Agones 适配器调用 POST /ready；健康探测 POST /health 独立周期执行。
6. 可选玩家人数 Counter 由同一次平台权威心跳采样发往 PATCH /v1beta1/counters/players；不信任客户端人数。OpenWorld/Village 首次有玩家时另向 SDK POST /allocate，防止 Agones 把活跃常驻实例作为 Ready 冗余副本回收。当前不自动把零玩家实例恢复 Ready，因为仍可能有有效转服票据；必须由 Go 控制面完成安全排空后再关闭。
7. 管理员或业务方先请求 BeginDrain，在本地完整排空后明确调用 CompleteDrain；适配器在 Stopped 状态 POST /shutdown。单独进入 Draining 不会立即杀掉有玩家的服务器。

注：SDK 状态变化异步生效。REST 2xx 只能表示 SDK 接受请求，真实 Ready/Allocated/Shutdown 仍需由 Kubernetes GameServer 状态观察确认。

调度公平性：健康检查由每实例 CoreTicker（核心计时器）按秒调度、最大 HTTP 请求时长为 2 秒；`AGONES_HEALTH_INTERVAL_SECONDS` 支持 1～8 秒。为避免 1 秒配置时持续发送 Health 而无法执行 Ready、Allocate、Counter，空闲调度先推进一次性操作和待更新容量；Health 如比原约定时间逾期超过 2 秒则抢占，Shutdown 永远具有最高优先级。这个策略并不承诺网络失联时按时收到 Sidecar 健康心跳，集群健康容忍窗口仍需真实压测。

安全身份：正式启动器拒绝空、非法或不适合客户端连接的 GameServer 端点（包括回环、未指定、组播、链路本地地址）、非法端口、异常 GameServer 名称，以及启动角色与只读服务器 Profile 不一致。避免通过固定容器端口或过期 Profile 继续发布可连接实例。
本机 SDK 发现请求不采用容器通用代理，禁止 HTTP 3xx 自动重定向；响应的 GameServer 身份、状态和 UDP 端口列表必须为预期 JSON 对象/列表。离线部署合同同时检查 Fleet 标签与启动环境、服务器角色 Profile 的一致性，防止 Go 分配选择器错误匹配到其他角色服务器。

后端原子分配保护：Go 适配器只允许通过 HTTPS Kubernetes CRD 发起 GameServerAllocation，拒绝令牌请求重定向、非 `game` 端口、重复游戏端口和不可路由端点；IPv6 使用方括号规范输出，避免客户端连接到错误服务器。由于 Agones 分配先于 Registry 绑定，若后者失败，仍须通过实际控制面/SDK 编排补齐孤儿 Allocated 实例回收方案。

**当前未闭环的结束事件：** `DBAServer` 的 `CompleteDrain` 已实现调用入口，但尚未核验竞技结果已持久提交、结算信息发送、跨服返回票据处理、连接清空后有生产调用方触发它。没有该调用就不会触发 SDK `/shutdown`。因此不能认为 MainArena 的赛后自动销毁已经可用；正式部署前必须由第三层服务器组合根接入权威比赛结束/失败通知，并通过端到端验证。不可用简单的赛程计时器代替玩家安全迁移。

## 4. 生产部署前置

此处 YAML 故意保留未解析的镜像占位符及 example-region、example.invalid 地址，不代表已发布或经过集群验收。执行者应在受控发布流程中：

- 用锁定 UE5.8 构建链制作可在 Linux 上运行的 DivineBeastsArenaServer 包，镜像须包含 Python 3、对应地图最小 Cook、Deploy/Server 角色 Profile、Deploy/Agones/launch_game_server.py，并保留路径 /opt/sslm。
- 改写三个 Fleet 中完全一致的 region、build-version、image、GAMESERVERCONTROL_BASE_URL、PLAYERDATA_BASE_URL 和 GAME_SERVER_PROTOCOL_VERSION；这些必须与生产控制面和构建收据一致，不得沿用示例。
- 在 sslm-game-servers 命名空间创建 sslm-server-control Secret（只包含 token 键），不可把实际令牌写入 Git。Go 业务后端使用独立 ServiceAccount sslm-game-allocator，并把 AGONES_NAMESPACE 设为 sslm-game-servers；默认 ServiceAccount 不具有任意集群管理权限。
- 确认节点外网地址/UDP 动态宿主端口可供实际客户端连接，服务器有足够的内存/CPU 与最小资源 Cook；portPolicy=Dynamic 不可以用容器内部端口 7777 充当客户端公网端口。
- Confirm Agones CountsAndLists（计数与列表）Beta 特性在目标版本已开启、CRD Schema 可用；若不可用，保留 MainArena Buffer Autoscaler，暂停两个 Counter Autoscaler。
- 验证常驻实例带玩家缩容保护、排空/断线重连/迁移；必要时先停用 Counter 自动缩容而仅增加副本，避免破坏真实玩家会话。
- 对 MainArena 执行 1v1～5v5 并行分配、实例创建、健康、Ready、Allocate、比赛结束 Drain/Shutdown 与 Fleet 回落；OpenWorld 和 Village 验证多玩家共享实例、总容量达阈值扩容与安全缩容。

建议执行顺序（先替换参数、确认镜像有效，然后审批应用，不在当前任务中擅自部署生产）：

    kubectl apply -f Deploy/Agones/allocator-rbac.yaml
    kubectl apply -f Deploy/Agones/fleet-main-arena.yaml
    kubectl apply -f Deploy/Agones/fleet-open-world.yaml
    kubectl apply -f Deploy/Agones/fleet-village.yaml
    kubectl apply -f Deploy/Agones/autoscalers.yaml
    kubectl -n sslm-game-servers get fleets,fleetautoscalers,gameservers

以上命令中的 kubectl（Kubernetes命令行工具）仅在目标集群权限、镜像与网络全部准备好后执行。本次创建源代码和配置不等于集群验证。

## 5. 验证工具与证据边界

静态合同测试（需 Python 3；使用 PyYAML 时还检查 YAML 语义）：

    python Tests/Architecture/TestAgonesDeployment.py

UE 原生回归入口（需要已装载平台服务器模块的编辑器自动化测试上下文）：

    GamePlatform.Server.Agones.BridgeScheduling

新测试只验证本地无网络副作用的调度选择；实际 Sidecar 状态收敛、Linux Dedicated Server 运行和 Kubernetes 扩缩仍须独立验证。

真实验收还包括 UE5.8 Dedicated Server 编译、Linux 服务器打包、Pod 状态、Agones Sidecar SDK 真实网络请求、Go ControlPlane 分配、容量阈值扩缩和 P95 排空耗时。未执行的项目必须标记为「待验证」，不能把脚本存在等同于通过。

来源：
- Agones REST SDK：https://agones.dev/site/docs/guides/client-sdks/rest/
- GameServer/Fleet：https://agones.dev/site/docs/reference/gameserver/
- FleetAutoscaler：https://agones.dev/site/docs/reference/fleetautoscaler/
