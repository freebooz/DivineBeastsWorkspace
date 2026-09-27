# OpenWorldUI（开放世界界面）

正式OpenWorld Experience只有：

- Experience.OpenWorld.Hub
- Experience.OpenWorld.Main

UI从ApplicationFlow View State读取WorldId/ExperienceId/RegionId，不读取或选择GameServerId、Shard（分片）、Instance（实例）或服务器Endpoint。

竞技入口只允许发送Command Intent（命令意图）；Fast Travel（快速旅行）未来也必须走业务Owner命令端口。当前没有项目OpenWorld Runtime插件，UI不能因此自建世界逻辑。

OpenWorld HUD当前可显示真实Health/Shield/Interaction/Region投影；Quest/Ability完整HUD数据仍待真实客户端端口。

OpenWorld实际Widget/HUD资产当前为0，视觉运行状态未执行。
