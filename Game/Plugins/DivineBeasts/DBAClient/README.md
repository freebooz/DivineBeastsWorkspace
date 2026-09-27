# DBAClient（神兽联盟客户端组合插件）

正式位置：`Game/Plugins/DivineBeasts/DBAClient/`。插件按模块宿主类型隔离客户端代码；虽然包含一个`Runtime`项目表现语义模块，服务器模块不得依赖任一`ClientOnly`模块。

| 模块 | 宿主 | 当前职责 |
| --- | --- | --- |
| `DivineBeastsApplicationFlowClient` | 客户端 | 登录后角色创建/选择、项目准入适配和流程状态 |
| `DivineBeastsPresentationRuntime` | 双端 | 项目表现上下文、目录和内容包稳定语义 |
| `DivineBeastsPresentationClient` | 客户端 | 项目表现适配与平台注册 |
| `DivineBeastsUIClient` | 客户端 | 项目界面契约、路由和ViewModel |

插件直接声明模块所需的`DBAGameplay`与平台插件依赖，不依赖DBAArena、GamePlatformArena或MobaPresentation。竞技客户端模块已迁入DBAArena；它单向消费本插件的公开流程扩展，注册与注销仍按GameInstance作用域处理。通用流程执行器、平台UI与VFX播放实现不归本插件。

之前引用的`DivineBeastsApplicationContracts.generated.hpp`在仓库和生成目录均不存在，代码未使用其中声明；本次移除了该孤立include。后端请求仍走现存Shared HTTP契约与实际HTTP适配代码。UE编译和接口联调仍待执行。

### 当前接入阻断（2026-09-27复核）

`DivineBeastsApplicationFlowClient`仍消费旧版Flow、Loading、Session接口；这不是仅修正模块名即可解决的问题。Flow已改为节点执行器／令牌合同，Loading提供`IGamePlatformLoadingService`，Session当前只有私有状态内核，尚无真实公开连接／准入服务。原生内核测试通过不代表本插件可编译、可登录或可进入世界。

在工作空间根运行`Tests/Architecture/ValidateProjectHeaders.ps1`会报告3个不存在头文件的4处引用并返回失败；它不能检查全部类型／方法兼容性。后续接线顺序、安全边界和证据见[工程缺项修复执行记录](../../../../Docs/Architecture/工程缺项修复执行记录.md)。禁止用旧名空壳、私有头导出或固定Admitted状态规避真实集成。

迁移前文档与旧描述快照见`Docs/Legacy/`；查看本README和正式工程规则判断现行职责。
