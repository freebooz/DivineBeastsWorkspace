# CharacterRosterUI（角色列表界面）

角色列表数据来自ApplicationFlow公开 CharacterRoster（角色列表）投影。

UI项包含：

- CharacterId：内部命令身份，不作为Shipping（正式版本）显示文本。
- HeroDefinitionId：内部Hero模板身份。
- DisplayName：玩家角色名称。
- AppearanceProfileId：逻辑外观Profile；当前真实ApplicationFlow摘要没有该字段，因此保持空。
- Status：角色状态。
- Selected/Enabled状态。

当前没有真实Hero Portrait（英雄头像）或UI内容包资产，不能展示伪造头像路径。

Roster为空时页面状态为Empty（空）；错误/重试由统一View State处理。UI不直接读取PlayerDataService。
