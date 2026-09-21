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

《神兽联盟》正式Dedicated Server（专用服务器）角色只有：OpenWorld（开放世界，含大厅/主城/野外）、Village（新手村）、MainArena（主竞技场）。

`OpenWorld.Hub（大厅/主城体验）` 是Experience，不是独立Lobby ServerRole（大厅服务器角色）。
