# DBAFrontEndPack（神兽联盟前端三维场景内容包）

本插件属于 DivineBeasts（神兽联盟项目层）的纯内容插件，只在 Client / Editor 目标启用。

## 资产职责

- `/DBAFrontEndPack/Maps/L_DBA_FrontEnd`：登录、角色选择、角色创建共用的本地前端宿主地图。
- `/DBAFrontEndPack/Maps/L_DBA_CharacterStudio`：角色选择/创建时按需流送的三维角色预览工作室。
- 后续可在 `CharacterPreview/Environment`、`Lighting`、`Materials`、`Audio` 下增加项目专属前端场景资源。

## 架构边界

- 不继承、不替代 `DBAWorlds（神兽联盟项目世界）`。
- 不创建 `UDivineBeastsWorldDefinition（神兽联盟世界定义）`。
- 不需要 ServerRole、World Assignment、Session Admission 或 Dedicated Server。
- 地图只承载视觉预览；ApplicationFlow 仍是唯一登录/角色/世界流程状态机。
- 预览 Actor 使用第一层 `AGamePlatformCharacterPreviewStage（平台角色预览舞台）`。
- 生肖 Mesh/Material/Animation 继续来自各 `DBAHeroPack_*` 的 `DA_Appearance_Zodiac_*`，本内容包不复制十二套角色资产。

## 端侧

- `DivineBeastsArenaClient Target（客户端目标）`：启用。
- `DivineBeastsArenaEditor Target（编辑器目标）`：启用。
- `DivineBeastsArenaServer Target（专用服务器目标）`：禁止启用。

因此本包不会进入 Dedicated Server 的 Cook / Stage。

## 启动地图说明

当前不在共享 `DefaultEngine.ini` 中设置 `GameDefaultMap（默认游戏地图）` 为 `L_DBA_FrontEnd`。原因是该配置同时被 Client / Server 读取，直接设置会破坏 Dedicated Server 的地图隔离。

现阶段 `L_DBA_CharacterStudio` 已由 `UDivineBeastsCharacterPreviewSubsystem` 根据 `CharacterEntry / CreateCharacter / ValidateSelection` 状态按需流送，因此角色三维预览不依赖全局默认地图。`L_DBA_FrontEnd` 作为正式客户端宿主资产已经交付；后续建立 Client-only 启动映射入口时再将其设置为客户端默认入口。
