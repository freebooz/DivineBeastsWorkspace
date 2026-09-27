# Shared（UE与Go共享契约层）

`Shared/Contracts` 是 UE5.8（虚幻引擎）与 Go Backend（Go业务后端）跨语言协议的唯一真源。

正式目录严格限定为：

```text
Shared/
├── Contracts/
│   ├── GamePlatform/
│   │   ├── OpenAPI/
│   │   ├── Proto/
│   │   ├── Schemas/
│   │   └── Events/
│   └── Games/DivineBeasts/
│       ├── OpenAPI/
│       ├── Proto/
│       ├── Schemas/
│       └── Events/
├── Generated/Cpp/
└── Docs/
```

- `Contracts/GamePlatform`：多游戏公共协议，不写死《神兽联盟》的ServerRole（服务器角色）、Experience（体验）或ArenaMode（竞技模式）枚举。
- `Contracts/Games/DivineBeasts`：《神兽联盟》项目专属协议。
- `Generated/Cpp`：唯一允许保存在Shared中的生成代码，仅供C++消费者使用，禁止手工修改。
- Go生成代码输出到 `Backend/generated`，不重复保存在Shared。

《神兽联盟》正式Dedicated Server（专用服务器）角色为OpenWorld（常驻世界）、Village（新手村）和MainArena（短生命周期主竞技场）。大厅是OpenWorld内的体验，不是独立服务器角色；角色与体验关系以 `Games/DivineBeasts/Schemas/server-catalog.schema.json` 的 `x-role-experience-map` 为唯一映射源；教学和训练是Village体验，不是独立服务器角色。

新大厅流量使用 `Experience.OpenWorld.Hub`。已发布的 `Experience.Lobby.Main` 仅作为兼容标识继续映射到OpenWorld；不得配置或注册 `GameServer.Role.Lobby`。
