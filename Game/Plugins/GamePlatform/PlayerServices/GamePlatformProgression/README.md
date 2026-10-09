# GamePlatformProgression

现行职责与依赖边界见 `Docs/Architecture/游戏端核心要求.md`、`Docs/Architecture/游戏端插件系统P0收敛审计.md` 与 `Game/Plugins/插件开发规范.md`；旧四服务器文档不再作为执行基线。


## 2026-09-30设计审查修订

OnViewChanged/GetViewGeneration覆盖首次、加载、错误、清空、轨道增删与定义变化；既有XP/等级事件保持，派生缓存不成为权威。新增UE事件用例未执行。

本次真实源码/Native/静态检查与未执行UE/后端/Cook边界见Game/Saved/Reviews/task2-repair-report.md；旧历史运行证据不自动覆盖本次修改。
