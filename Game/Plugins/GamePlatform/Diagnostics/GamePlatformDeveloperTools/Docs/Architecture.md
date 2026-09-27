# Architecture（架构）

GamePlatformDeveloperTools（游戏平台开发者工具插件）只有一个 Editor 模块：GamePlatformDeveloperTools。

边界：
- GameFoundation 平台层，不依赖 MobaCommon 或 DivineBeasts。
- Client/Server Target 始终禁用本插件。
- Editor Target 仅非 Shipping 配置启用。
- Commandlet 运行时无 Client、Game World、LocalPlayer、Slate Modal 依赖。
- UI 菜单与 Headless Validation（无界面验证）分离。

核心链路：架构规则 → Validator（验证器）→ UE Data Validation → Commandlet → CI Gate → Evidence Report（证据报告）→ Manual Review（人工审查）→ Release Gate（发布门禁）。