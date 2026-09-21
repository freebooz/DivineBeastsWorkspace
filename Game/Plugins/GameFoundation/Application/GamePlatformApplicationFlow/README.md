# GamePlatformApplicationFlow（游戏平台应用流程插件）

版本：0.1.0｜状态：一期必建，流程执行代码已实现，UE 工程验收待完整环境验证。

当前证据：原生 Debug／Release 各 21 场景通过；UE5.8 UHT 和全部插件 C++ 编译通过。Editor 模块链接因引擎缺少 UnrealEditor-Core.lib 失败，28 项 UE 自动化尚未运行，不能标记为可发布成品。详见下方验证文档。

本插件提供一个 GameInstance 作用域的主流程执行器。项目组合根注入节点对象及转换图，平台层不包含登录、选角、地图路径、项目身份、HTTP 地址或竞技依赖。

执行链：组合根 Configure → Start 返回作用域句柄 → 节点 Execute → 任意线程完成投递 → 游戏线程处理 → 节点 Finish → 分支／退避／终态。

## 能力与边界

- 事务式配置校验：空入口、重复节点／实例、悬空边、不可达节点、循环、非法超时与重试配置均拒绝，保留旧配置。
- 默认成功边和具名成功分支；终点使用 NAME_None，未知分支明确失败。
- 节点异步完成、有限次数重试、固定退避、单调时钟超时、取消及作用域关闭。
- 作用域＋运行代次句柄；尝试代次邮箱拒绝迟到或重复回调。只保存一个完成结果，避免重复回调堆积。
- 每次开始的尝试恰好清理一次；终态事件每次运行只广播一次。
- GameInstance 级跨地图生命周期及 GC 可追踪节点／载荷保活；空闲时没有 Ticker。
- UGamePlatformCallbackFlowNode 支持组合根直接注入执行与清理函数，也可实现 UGamePlatformFlowNode 派生类型。

当前 C++ 流程图是有限无环图；节点级有限重试不依赖图循环。没有蓝图流程编辑器、并行节点调度、自动事务回滚、磁盘恢复或网络复制。玩家在世界中匹配等并行业务由各领域服务执行，主流程按需等待其结果。

## 模块、端侧与启用

唯一模块名 `GamePlatformApplicationFlow`，Runtime，Default 加载。支持 Editor／Client／Server；普通 Game 目标也允许，因为代码只依赖 Core、CoreUObject、Engine，且不含任何客户端／服务器专属业务。该允许范围是本插件边界设计，尚无四目标构建通过证据。

消费方 `.Build.cs` 声明 `GamePlatformApplicationFlow` 模块依赖；若消费方也是插件，其 `.uplugin` 同时声明本插件依赖。真实 `.uproject` 的 Plugins 列表显式启用：

```json
{"Name": "GamePlatformApplicationFlow", "Enabled": true}
```

本次未重写工作空间中为空的主工程与 Target 文件。该问题需要主工程任务独立处理。本插件无 Content、无配置扫描、无资产包或 GameplayTag 注册要求，不创建伪造 .uasset。

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

上述测试编译的是插件真实生产调度核心，不包含 UHT／UE 子系统。UE 自动化入口 `GamePlatform.ApplicationFlow` 包含 21 项核心场景及 7 项适配层场景（含 5 项回调内关闭回归）。UE 构建脚本及验证边界见 [测试与验证说明](Docs/测试与验证说明.md)。

## 文档

- [架构与生命周期说明](Docs/架构与生命周期说明.md)：职责、线程、所有权与状态合同。
- [接口与接入说明](Docs/接口与接入说明.md)：配置、节点实现、错误、重试与组合根示例。
- [目录规划说明](Docs/目录规划说明.md)：全部文件的中文职责及维护要求。
- [测试与验证说明](Docs/测试与验证说明.md)：行为用例、命令、证据与未验证事项。
- [神兽联盟历史对话与插件工程实现参考](../../../../../Docs/Architecture/神兽联盟历史对话与插件工程实现参考.md)：当前采用的历史要求与后续插件职责。
