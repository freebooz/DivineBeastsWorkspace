# GamePlatformVillage（游戏平台通用新手体验候选插件）

当前状态：**条件保留／未实现**。现有 Runtime、ClientOnly、ServerOnly 三个模块均只有模块注册入口，真实活动源码中没有外部消费者。

Village 是《神兽联盟》的正式服务器角色，但“服务器角色”不等于平台必须存在 Village 专用机制插件。教学／训练应优先由 GamePlatformWorld、GamePlatformGameplay、GamePlatformQuest、GamePlatformInteraction、AI／Navigation 与项目 Definition 组合。

本插件只有在后续证明存在跨游戏通用、独立生命周期和独立测试边界的新手体验机制时才实施；否则列为退休／合并候选。P0 不擅自删除稳定身份。

现行规则：
- [游戏端核心要求](../../../../../Docs/Architecture/游戏端核心要求.md)
- [插件系统P0收敛审计](../../../../../Docs/Architecture/游戏端插件系统P0收敛审计.md)
- [插件开发规范](../../../插件开发规范.md)

当前不得作为已实现或生产可用能力。
