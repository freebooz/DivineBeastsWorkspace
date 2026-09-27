# Versioning（协议版本规范）

当前公共契约版本：`2.0.0`。

兼容规则：

1. OpenAPI（HTTP接口）新增可选字段属于向后兼容；删除字段、改变字段语义必须提升主版本。
2. Proto（Protocol Buffers协议）发布后的字段编号禁止复用；删除字段必须使用 `reserved` 保留编号与名称。
3. JSON Schema（JSON结构约束）新增必填字段视为破坏性变更。
4. `GamePlatform` 与 `Games/DivineBeasts` 分别维护语义边界，但统一进入本Shared版本发布流程。
5. 角色、体验等字符串枚举新增值按兼容性评审递增次版本；删除或改变已发布值语义按破坏性变更处理。本次因取消独立 `GameServer.Role.Lobby` 将契约主版本提升至2.0.0。大厅正式使用 `Experience.OpenWorld.Hub` 并由 `GameServer.Role.OpenWorld` 承载；已发布 `Experience.Lobby.Main` 保留为兼容标识，同样映射到OpenWorld，不再作为新Profile默认值。
