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

`DivineBeastsApplicationFlowClient`的Loading消费者已开始迁移到`IGamePlatformLoadingService`：使用完整Loading句柄、精确订阅、逐操作任务工厂和显式资源释放；六项就绪事实绑定观察GUID，世界逻辑身份通过Data租约加载`UDivineBeastsWorldDefinition`，再以其`MapIdentity`核验当前实例世界。`MapId`是诸如`Map.MainArena.Default`的逻辑标识，不可直接当作地图包路径；当前共享模型没有声明MapId到世界定义的本地映射，客户端依世界定义与WorldId核验地图包，分配中的MapId仍由后端／会话使用。

Flow消费者仍调用已移除的旧执行器API；Flow现行服务要求定义资产、节点工厂和令牌事件，尚未完成迁移。Session当前只有私有状态内核，尚无真实公开连接／准入服务。以上改动尚未运行UE UHT、编译或加载自动化，不能据此声称项目模块可编译、可登录或可进入世界。

前次运行`Tests/Architecture/ValidateProjectHeaders.ps1`曾报告3个不存在头文件的4处引用。此后Loading旧头引用已改为真实公开接口；按当前源码检视，Flow旧头仍有2处、Session旧头1处，共2个头名3处引用待修。预检尚未重跑，且该工具不能检查方法签名兼容性。禁止用旧名空壳、私有头导出或固定Admitted状态规避真实集成。

迁移前文档与旧描述快照见`Docs/Legacy/`；查看本README和正式工程规则判断现行职责。
