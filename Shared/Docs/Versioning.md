# Versioning（协议版本规范）

当前公共契约版本：`1.3.0`。

兼容规则：

1. OpenAPI（HTTP接口）新增可选字段属于向后兼容；删除字段、改变字段语义必须提升主版本。
2. Proto（Protocol Buffers协议）发布后的字段编号禁止复用；删除字段必须使用 `reserved` 保留编号与名称。
3. JSON Schema（JSON结构约束）新增必填字段视为破坏性变更。
4. `GamePlatform` 与 `Games/DivineBeasts` 分别维护语义边界，但统一进入本Shared版本发布流程。
