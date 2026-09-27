# CharacterIdentityLayers（角色身份分层）

必须保持：

- PlayerId：后端账号/玩家身份。
- CharacterId：PlayerData持久角色档案ID。
- HeroDefinitionId：十二生肖模板ID。
- Pawn/Avatar：当前World中的ACharacter实例。
- SpawnGeneration：Pawn生成代次。
- AvatarGeneration：Avatar绑定代次。
- ArenaRosterSlot：竞技场单场参赛槽位，归Arena。

UDivineBeastsCharacterComponent仅保存当前运行绑定，不写数据库。CharacterId在服务器可信Spawn/Admission上下文中注入，对网络只OwnerOnly复制；HeroDefinitionId、公开生肖身份和必要Generation可供远端观察者使用。
