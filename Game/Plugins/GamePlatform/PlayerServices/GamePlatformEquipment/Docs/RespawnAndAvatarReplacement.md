# RespawnAndAvatarReplacement（重生与角色替换）

Persistent Equipment Snapshot 不随 Pawn/Avatar 重生丢失。EquipmentServer 的 BindAvatar（绑定角色）比较 ASC（能力系统组件）和 AvatarGeneration；变化时先撤销旧槽 GrantHandle，再按现有 Snapshot 重新应用。

EquipmentClient 的 BindAvatar 同样提升视觉代次，取消旧资源加载并清理旧 Mesh，再对新 Avatar 重新挂接。

当前工程没有确定的 PlayerState-owned ASC 与 Character-owned ASC 正式生产模式，因此两种 Respawn 场景的真实重复授予验证均未执行。