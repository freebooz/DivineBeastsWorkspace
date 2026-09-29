# MigrationAndHandover（迁移与交接）

当前唯一正式 Interaction（交互）身份为 `Game/Plugins/GamePlatform/World/GamePlatformInteraction`。本插件属于 GamePlatform（平台层）世界能力分类，维护一个 Runtime（共享运行时）模块；不再以历史设计稿中的 GameFoundation/Gameplay 等逻辑路径描述真实磁盘位置。

本次审查不移动插件、不改公开插件身份，只增量收敛真实目录、Category（分类）、模块依赖和运行语义。服务器 Authority（权威）逻辑继续位于共享 Runtime 模块，通过网络角色判断执行；未恢复无独立职责的 `GamePlatformInteractionServer（交互服务器模块）`。

`GamePlatformGameplay（通用玩法）`仅作为 Private（私有实现）依赖提供 Active/AvatarGeneration（玩法活跃/角色代次）事实，Interaction 的 Public（公开）契约不暴露其类型；完整登录、准入、出生、死亡、重生流程仍由各自领域及组合根负责。

后续正式 Character/Input/Online/UI（角色/输入/在线/界面）接入必须继续通过公开 Provider（提供者）、只读事件和组合层完成，不能反向让 Interaction 依赖这些上层或客户端专有模块。新增项目专属生肖、竞技规则、奖励或表现逻辑时，应分别落在 DivineBeasts/MobaCommon/相应平台插件中，而不是继续膨胀本插件。
