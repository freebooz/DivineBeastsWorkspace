# GamePlatformLobby（游戏平台通用大厅体验插件）

当前状态：**条件保留／未实现**。现有 Runtime、ClientOnly、ServerOnly 三个模块均只有模块注册入口，真实活动源码中没有外部消费者。

正式服务器角色只有 OpenWorld、Village、MainArena；Lobby 不是独立 ServerRole、Server Target 或世界包。现行工程规则暂时保留本插件的稳定通用体验身份，因此本轮不删除，但不得据此恢复独立 Lobby 服务器。

只有未来形成可跨游戏复用且不能由 World／ApplicationFlow／UI／Party 等现有能力表达的“非权威大厅体验机制”时，才允许补实现；否则应在明确基线变更和引用迁移后退休。

现行规则：
- [游戏端核心要求](../../../../../Docs/Architecture/游戏端核心要求.md)
- [插件系统P0收敛审计](../../../../../Docs/Architecture/游戏端插件系统P0收敛审计.md)
- [插件开发规范](../../../插件开发规范.md)

本插件当前不得标记 G1～G5 完成，不得作为生产启用能力。
