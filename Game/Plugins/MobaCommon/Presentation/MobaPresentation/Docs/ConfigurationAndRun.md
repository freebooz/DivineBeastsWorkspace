# ConfigurationAndRun（配置与运行）

正式工程仍为 Game/DivineBeastsArena.uproject（神兽联盟唯一正式UE工程），已启用 MobaPresentation（MOBA表现语义插件）。

MobaPresentationRuntime为Runtime（运行时）模块；MobaPresentationClient为ClientOnly（仅客户端）模块。Dedicated Server允许Runtime参与Tag/类型编译，但不加载Client模块。

当前没有必须的Content资产和后端环境变量。Native Gameplay Tags（原生玩法标签）由Runtime代码注册。

实际运行前仍需UE5.8 Client/Server/Editor编译验证、Tag Dictionary验证以及Client/Server Cook验证。
