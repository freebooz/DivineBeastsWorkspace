# DiscoveryAndFocus（候选发现与焦点）

本地客户端按 `FocusRefreshInterval`使用 PlayerController ViewPoint（玩家控制器视点）执行一次 ECC_Visibility 射线，只把命中 Actor 上的 `UGamePlatformInteractableComponent`作为候选。Focus 变化不发服务器 RPC。

同一 Target 的 Option 通过 `FGamePlatformInteractionFocusRules`稳定选择：先过滤本地 Enabled/结构合法/距离，再比较 Priority，高优先级优先；同优先级时较短 MaxDistance 优先，最后使用 FName 的稳定字典序作为 Tie Break。UE5.8 `FName::LexicalLess`提供跨进程稳定字母顺序。

当前第一版是单视线候选发现，不做每帧世界 Actor 扫描，也没有实现多 Actor Overlap 候选池。相同 Trace 命中和 Option 输入会得到稳定 Focus。

FocusSnapshot 只属于本地体验提示，不是服务器授权事实。
