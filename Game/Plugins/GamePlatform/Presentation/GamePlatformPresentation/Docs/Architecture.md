# Architecture（架构）

GamePlatformPresentationCore定义FGamePlatformPresentationRequest、Context、Catalog Fragment和确定性解析数据；GamePlatformPresentationClient以ULocalPlayerSubsystem作为组合入口。

Client Subsystem支持Provider注册/注销、Context Contributor、Catalog Fragment、BuildContext、Catalog Resolve和Submit。Provider按Priority→ProviderId确定性排序；World cleanup推进WorldGeneration，使旧世界请求Fail Closed。

具体VFX/SFX/UI模块只能作为Provider消费中立请求，Presentation本身不依赖具体表现资产类型。

## CharacterPreview（角色三维预览）

`AGamePlatformCharacterPreviewStage` 属于 ClientOnly 通用表现能力。它只提供预览 Mesh、Camera、Yaw 旋转和镜头距离，不负责资产目录、角色业务状态、网络复制、GAS、Combat、Inventory 或 AI。项目层必须先异步解析自己的 Appearance/Profile，再把已加载资源交给 Stage。

该类可放入项目自己的 FrontEnd/Studio 地图，但地图内容不归 GamePlatformPresentation 所有。这样平台机制可跨项目复用，而项目地图、美术、灯光与角色外观仍归项目内容包。