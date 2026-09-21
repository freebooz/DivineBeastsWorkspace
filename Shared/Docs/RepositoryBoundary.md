# RepositoryBoundary（仓库边界规范）

## Shared职责

`Shared` 只解决 UE 与 Go 跨语言边界：

- `Contracts/GamePlatform`：多游戏公共协议。
- `Contracts/Games/DivineBeasts`：《神兽联盟》项目协议扩展。
- `Generated/Cpp`：唯一共享C++生成绑定。
- `Docs`：版本、权限、生成和编译映射。

禁止放入：UE Gameplay实现、Go领域实现、部署密钥、数据库仓储代码、编辑器资产、第二份协议定义。

## Backend职责

`Backend` 是纯Go业务后端，保持一个Go Module（Go模块）和当前真实内部布局：

- `cmd`：五个薄应用入口。
- `internal`：领域、应用编排、平台适配和仅Go内部协议。
- `pkg`：确有多个真实复用者的公共Go包。
- `generated`：Go绑定生成代码。
- `migrations`：数据库迁移唯一来源。
- `configs`：非敏感配置。
- `tests`：后端跨模块和架构测试。

禁止重新增加与上述目录平行的 `gameplatform/`、`games/`、`Infrastructure/`、`Deployment/` 等后端根目录。

## 与完整仓库的关系

- UE正式工程只有 `Game/DivineBeastsArena.uproject`。
- UE插件源码只进入 `Game/Plugins/`。
- 仓库构建入口统一归 `Build/`。
- 部署配置统一归 `Deploy/`。
- 跨UE/Go/服务器系统级测试统一归仓库根 `Tests/`；`Backend/tests` 只负责Go后端自身测试。
