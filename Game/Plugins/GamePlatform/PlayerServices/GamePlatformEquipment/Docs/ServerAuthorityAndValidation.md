# ServerAuthorityAndValidation（服务器权威与校验）

Equip/Unequip 只能由权威服务器组件发起。后端事务再次校验玩家 InventoryRevision、ItemInstance 所有权、Item active 状态、ItemDefinition→EquipmentDefinition 映射、Slot兼容性和 EquipmentRevision。

DBAServer 请求携带 Internal Token、X-Game-Server-Id 与 X-Bound-Player-Id。当前仓库仍缺 GameServerControl/Session 提供的真实 ServerInstance↔Player 绑定验证，也没有权威 CharacterId↔PlayerId 角色归属表，因此这两项运行安全验证未完成。

普通客户端不能提交属性、Effect Class、Mesh 或 Socket。