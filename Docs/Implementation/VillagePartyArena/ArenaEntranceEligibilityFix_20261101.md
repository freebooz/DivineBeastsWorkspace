# 竞技入口服务器资格适配修复

用户报告新版新手村竞技门点击和 F 无反应。当前客户端日志已记录同一门的 `Requested=1`；在真实运行客户端读取 LastResult，服务器返回 `InteractorNotActive`，没有创建 Session。前轮点击与远镜头发现修复已存在，不能把请求已发送当作准入成功。

根因：GamePlatformInteraction 通过 IGamePlatformGameplayEligibilityProvider 读取资格。平台 Experience 世界的唯一 Active 权威在 GameMode / PlayerState 生命周期中；平台 PlayerController 尚未实现该接口，因此查询回退到角色上给非 Experience 宿主使用的中立 Eligibility 组件，该组件默认失活。移动和施法读取的现有生命周期已经 Active，两个读取入口没有适配到同一权威。

实施顺序：

1. 在现有离线 HostLifecycle 原生回归中，使用真实准入、准备、撤销和竞技门，验证准备前拒绝、Active 后成功提交、撤销后拒绝；先保留失败证据。
2. 由 GamePlatformPlayerControllerBase 实现既有资格接口，只读当前 GameMode 的 IsPlayerGameplayActive 与 PlayerState 的当前 PawnGeneration。不增加复制字段、状态机或可由客户端写入的 Active。世界退出、无当前 Pawn、代次越界均失败关闭。项目 WorldPlayerController 进一步复用角色已有 ASC EvaluateActivationEligibility，保持 Ready、角色代次、死亡与当前权威检查；死亡后的五秒重生等待不能仅因平台记录仍 Active 而开启门交互。现有非平台宿主的 Pawn 资格路径不变。
3. 执行受影响原生门禁，固定既定参数增量构建完整 Editor、Client、Server；对相同输入 Cook / Stage 双端，核对保护清单中的当前 216 份法术资源。
4. 受控替换实际 Village / MainArena，核对真实 PID、包路径、启动身份与共享战斗版本后启动两个客户端。分别验证真实门交互、竞技面板和正常匹配准入，不强制旅行或伪造匹配。

证据位于 Saved/Validation/ArenaEntranceRuntime20261101。保留主分支现有未提交修改，不提交或清理缓存；共享编辑器的地图、未保存状态和镜头已单独记录。用户已在两个相关聊天直接授权临时关闭 PID 65604 并恢复，执行前原生再次核对无脏包后正常退出。

RED：完整 Editor 14eb661a-d261-47bc-807f-1de0caf39a56 通过；原生 HostLifecycle a7ea5249-e0e5-476b-bc13-9d55be786520 经真实离线准入和当前准备令牌转为 Active 后，竞技门仍以错误终态拒绝，且资格接口为空，符合实机故障。前两次夹具修正（Controller 隐藏位置 API、离线基类需要显式组合交互组件）及 DLL 锁定失败均保留原始证据，不计为有效 GREEN。

代码复审确认必须保留死亡/Ready门禁，已通过项目派生覆写连接现有GAS查询。当前生产改动为平台只读资格适配和项目既有角色资格组合；GREEN、完整双目标、双端Cook和真实准入仍待验证。
