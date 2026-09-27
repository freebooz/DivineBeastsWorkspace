# Development（战斗开发测试资产）

此目录用于通过 Unreal Editor（虚幻编辑器）真实创建 `GE_Combat_Test_Damage`、`GE_Combat_Test_Healing`、`GE_Combat_Test_ShieldAdd`、`GE_Combat_Test_Stun`、`GE_Combat_Test_Silence`、测试 Ability（技能）和中立网络测试资产。

当前 Runner 没有可用 UE5.8 工具链，因此本轮没有创建伪造 `.uasset/.umap`。相关资产、测试地图、Client/Server Cook 与双客户端 Dedicated Server（专用服务器）联调均保持未执行。

`Root（定身）`本轮未实现，因为 GamePlatformCharacter（平台角色插件）尚无经过验证的 MovementBlocked（移动阻止）适配接口。
