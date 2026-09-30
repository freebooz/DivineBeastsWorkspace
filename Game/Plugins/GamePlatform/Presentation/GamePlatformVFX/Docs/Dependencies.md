# Dependencies（依赖边界）

平台层依赖方向：

```text
GamePlatformVFXClient
→ GamePlatformData
→ GamePlatformCore

GamePlatformVFXClient (Private Integration)
→ GamePlatformPresentationCore / Client

GamePlatformVFXClient
→ Niagara / Engine
```

Public Header 未暴露 Presentation 类型，因此 `GamePlatformPresentationCore` 已收敛到 Private Dependency。

禁止：GamePlatformVFX 反向依赖 MobaCommon 或 DivineBeasts 的类、模块和资产；禁止 Dedicated Server 依赖 GamePlatformVFXClient / Editor。

当前外部边界已经落地：

1. `GamePlatformData` 提供统一 Definition/World Lease，VFX 不再复制 Definition Loader；
2. `GamePlatformPresentation` 提供正式 Provider 注册和 Catalog Resolve，VFX 不再建立第二套标准语义 Catalog。

`DBAClient` 只作为上层客户端组合根启用 GamePlatformVFX，不改变依赖方向。
