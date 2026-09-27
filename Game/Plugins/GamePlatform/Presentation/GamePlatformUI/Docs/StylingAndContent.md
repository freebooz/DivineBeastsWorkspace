# StylingAndContent（样式与内容）

`Content/Client/Defaults`用于跨游戏默认样式与降级资源；`Templates`用于 Root Layout/Screen/Dialog/Toast 模板；`Development`仅用于测试和核验。

所有 `.uasset/.umap`必须由 Unreal Editor 创建，本轮只创建目录 README，不伪造二进制资产。

GamePlatformUI 不允许包含 DivineBeasts、生肖、英雄、世界、皮肤等项目美术身份。项目公共 UI 美术应进入后续 `DBAUIPack_Core（神兽联盟公共UI资产包）`。

Shipping 包必须排除 Development 资源。
