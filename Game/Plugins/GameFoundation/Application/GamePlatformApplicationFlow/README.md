# GamePlatformApplicationFlow（游戏平台应用流程插件）

版本：0.1.0｜状态：一期必建，流程执行代码已实现，UE 工程验收待完整环境验证。

当前源码已增加M0资产流程兼容扩展，仍只使用同一个执行器。旧版21场景与历史UE编译记录不能证明本次扩展已通过UE验证；本次命令、场景与边界见 [M0兼容扩展与验证](Docs/M0兼容扩展与验证.md)。本轮未运行UBT、UHT或UE自动化，未绕过现存空描述文件阻断。

本插件提供一个 GameInstance 作用域的主流程执行器。项目组合根注入节点对象及转换图，平台层不包含登录、选角、地图路径、项目身份、HTTP 地址或竞技依赖。

执行链：组合根 Configure → Start 返回作用域句柄 → 节点 Execute → 任意线程完成投递 → 游戏线程处理 → 节点 Finish → 分支／退避／终态。

## 能力与边界

- 旧Configure保持DAG和事务式校验；新增流程资产只在显式允许时接纳循环，使用即时循环预算约束同步重访。
- 默认成功边和具名成功分支；终点使用 NAME_None，未知分支明确失败。
- 节点异步完成、有限次数重试、固定退避、单调时钟超时、取消及作用域关闭。
- 作用域＋运行＋节点＋NodeGeneration令牌；外部事件与回调共用原邮箱，首次有效完成生效。
- 每次开始的尝试恰好清理一次；终态事件每次运行只广播一次。
- GameInstance 级跨地图生命周期及 GC 可追踪节点／载荷保活；空闲时没有 Ticker。
- UGamePlatformCallbackFlowNode 支持组合根直接注入执行与清理函数，也可实现 UGamePlatformFlowNode 派生类型。

旧C++流程图仍为DAG；资产模式通过Data就绪租约读取UGamePlatformFlowDefinition，工厂为每run创建独立节点，输入定义身份传入上下文，终态释放所接管租约。没有蓝图流程编辑器、并行节点调度、自动事务回滚、磁盘恢复或网络复制。玩家在世界中匹配等并行业务由各领域服务执行。

## 模块、端侧与启用

唯一模块名 `GamePlatformApplicationFlow`，Runtime，Default 加载。支持Editor／Client／Server／Game；依赖Core、CoreUObject、Engine及平台GamePlatformCore、GamePlatformData，模块规则与插件描述同时声明真实依赖。没有客户端／服务器专属业务，也尚无本次四目标构建通过证据。

消费方 `.Build.cs` 声明 `GamePlatformApplicationFlow` 模块依赖；若消费方也是插件，其 `.uplugin` 同时声明本插件依赖。真实 `.uproject` 的 Plugins 列表显式启用：

```json
{"Name": "GamePlatformApplicationFlow", "Enabled": true}
```

主工程与Target由父任务处理。本插件提供流程资产类型但不内置Content或扫描目录；具体资产由宿主通过引擎生成，并由Data统一发现和加载，不创建伪造 .uasset。

## 最小接入

组合根包含公开头后，通过自己的 GameInstance 获取唯一服务：

```cpp
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Interfaces/GamePlatformCallbackFlowNode.h"
#include "Engine/GameInstance.h"

// GameInstance 必须来自调用方的真实作用域；不要使用 GWorld 或固定第零号玩家。
UGamePlatformApplicationFlowSubsystem* Flow =
    GameInstance->GetSubsystem<UGamePlatformApplicationFlowSubsystem>();
```

完整函数式装配示例见 [接口与接入说明](Docs/接口与接入说明.md)。项目层传入真实 Execute／Finish 函数，例如 Online 请求、Session 准入、Loading 就绪；本插件不提供返回固定成功的登录替身。

## 验证入口

从工作空间根执行（Windows，需 CMake 和 Visual Studio C++ 工具）：

```powershell
& 'Game/Plugins/GameFoundation/Application/GamePlatformApplicationFlow/Tests/Scripts/TestApplicationFlow.ps1' -Configuration Debug
& 'Game/Plugins/GameFoundation/Application/GamePlatformApplicationFlow/Tests/Scripts/TestApplicationFlow.ps1' -Configuration Release
```

上述测试编译的是插件真实生产调度核心，不包含UHT／UE子系统。UE入口GamePlatform.ApplicationFlow加入资产校验、工厂、事件与真实租约场景；真实租约用例必须提供 `-GamePlatformFlowTestDefinition=GamePlatformDefinition:foundation.flow@1` 并实际生成资产，缺失明确失败。本轮数量与结果以 [M0兼容扩展与验证](Docs/M0兼容扩展与验证.md) 为准。

## 文档

- [架构与生命周期说明](Docs/架构与生命周期说明.md)：职责、线程、所有权与状态合同。
- [接口与接入说明](Docs/接口与接入说明.md)：配置、节点实现、错误、重试与组合根示例。
- [目录规划说明](Docs/目录规划说明.md)：全部文件的中文职责及维护要求。
- [测试与验证说明](Docs/测试与验证说明.md)：行为用例、命令、证据与未验证事项。
- [神兽联盟历史对话与插件工程实现参考](../../../../../Docs/Architecture/神兽联盟历史对话与插件工程实现参考.md)：当前采用的历史要求与后续插件职责。
