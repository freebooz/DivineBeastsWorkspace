# DiscoveryAndFocus（候选发现与焦点）

本地客户端按 `FocusRefreshInterval（焦点刷新间隔）`使用 PlayerController ViewPoint（玩家控制器视点）执行一次 ECC_Visibility（可见性碰撞通道）射线，只检查该次射线命中 Actor（实体）上的全部 `UGamePlatformInteractableComponent（可交互组件）`；不会扫描世界 Actor。Focus（焦点）变化不发送服务器 RPC（远程过程调用）。

候选通过 `FGamePlatformInteractionFocusRules（交互焦点规则）`稳定选择：先过滤 Target/Option Enabled（目标/选项启用状态）、结构合法性、采集剩余次数、局部距离以及全局 MaxDistance/MaxHoldDuration（最大距离/最大长按时长）约束；再比较 Priority（优先级），高优先级优先；同优先级时较短 MaxDistance 优先，再使用 `FName::LexicalLess（名称字典序）`稳定排序；选项排序仍完全相同时，优先实际交互点距离更近的组件。

当前仍采用单视线候选发现，不做每帧世界 Actor（实体）扫描，也没有建立多 Actor Overlap（重叠）候选池。Occupancy（并发占用数）不作为客户端最终授权门槛：客户端可能仍显示刚变忙的目标，服务器会清理失效弱会话并执行最终 Exclusive/Shared（独占/共享）并发判定，从而避免复制延迟或失效弱引用把交互永久锁死。

FocusSnapshot 只属于本地体验提示，不是服务器授权事实。
