# Architecture（总体架构）

正式路径为 `Game/Plugins/MobaCommon/GamePlatformArena`。本插件位于MOBA通用层，依赖方向固定为 `ArenaClient/ArenaServer → Arena → MobaData → MobaCore → GamePlatform（游戏平台层）`。Client（客户端）和Server（服务器）模块互不依赖，MobaCommon不得反向依赖DivineBeasts（神兽联盟项目层）。本次只迁移目录，不改变插件或五模块身份；项目竞技适配归可选DBAArena。

所有竞技模式共用 `GameServer.Role.MainArena（主竞技场服务器角色）`、同一Server Target（服务器构建目标）和同一Server Binary（服务器二进制），禁止按1v1～5v5拆服务器。Arena事实层不依赖Presentation（表现层），UI/VFX/SFX由上层读取竞技事实。
