# CharacterSelectionUI（角色选择界面）

Persistent Character Selection（持久角色选择）与 Arena Hero Selection（竞技英雄选择）是两个不同命令：

- SelectPersistentCharacter：使用CharacterId选择玩家长期档案角色。
- ArenaSelectHero：使用HeroDefinitionId选择单场竞技Hero。

持久角色选择成功并不意味着可以直接Travel（切服）；还必须经过后端Selection验证、World Assignment（世界分配）、TransferTicket（迁移票据）、Session Admission（会话准入）和WorldReady（世界就绪）。

UI没有ClientTravel调用，也不能自行把角色标为已选。
