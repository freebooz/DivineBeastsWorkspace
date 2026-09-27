# ServerRolesAndExperiences（服务器角色与体验）

正式ServerRole仅三个：

- GameServer.Role.OpenWorld（常驻世界）
- GameServer.Role.Village（新手村）
- GameServer.Role.MainArena（主竞技场）

GameServer.Role.Lobby不是正式ServerRole。

正式Experience共六个：Experience.OpenWorld.Hub、Experience.OpenWorld.Main、Experience.Village.Main、Experience.Village.Tutorial、Experience.Village.Training、Experience.MainArena.Main。

映射由Shared Catalog生成：OpenWorld.Hub/Main→OpenWorld；Village.Main/Tutorial/Training→Village；MainArena.Main→MainArena。

历史DBAServer/RoleBindings/Lobby物理目录可以保留作为Migration/Deprecation（迁移/废弃）记录，但不得重新注册为正式服务器角色。
