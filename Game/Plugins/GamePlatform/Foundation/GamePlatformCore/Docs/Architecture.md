# GamePlatformCore（游戏平台核心）插件设计

> 版本基线：2026-09-27。
> 正式位置：`Game/Plugins/GamePlatform/Foundation/GamePlatformCore/`。
> 本文是 GamePlatformCore（游戏平台核心）当前设计、边界和扩展规则的正式入口；真实 API 以 Public（公开接口）头文件为准，验证证据见 [TestingAndEvidence.md](TestingAndEvidence.md)。

## 1. 定位

GamePlatformCore 是 `GamePlatform（游戏平台层）` 最底层稳定契约插件，只提供跨游戏、跨端都需要的轻量值类型和日志分类。

依赖方向：

```text
DivineBeasts（项目层）
        ↓
MobaCommon（MOBA通用层）
        ↓
GamePlatform各领域插件
        ↓
GamePlatformCore（游戏平台核心）
```

GamePlatformCore 仅依赖 UE 的 `Core（核心模块）` 与 `CoreUObject（反射基础模块）`，不依赖 Engine（引擎高层模块）、GamePlatformData（平台数据）、World（世界）、Online（在线）、MobaCommon 或 DivineBeasts。

## 2. 负责与不负责

负责：

- `FGamePlatformId（平台逻辑身份）`：跨系统稳定逻辑身份。
- `FGamePlatformErrorCode（平台结构化错误码）`：稳定机器错误身份与错误域。
- `FGamePlatformVersion（平台三段版本）`：纯版本值及比较。
- `FGamePlatformVersionRange（平台版本兼容区间）`：显式闭区间接纳值对象。
- `FGamePlatformResult（平台统一结果）`：最小公共调用结果。
- `LogGamePlatformCore（平台核心日志分类）`。

不负责：

- Asset Manager（资产管理器）、Primary Asset（主资产）、Lease（租约）或资源加载。
- 世界、玩家、会话、网络、HTTP、数据库、后端连接。
- Gameplay（玩法）、GAS（游戏能力系统）、MOBA、生肖或项目规则。
- UI（用户界面）本地化文案。
- 自动决定协议、内容或插件版本是否兼容。
- 进程级 Singleton（单例）、Subsystem（子系统）或 Manager（管理器）。

核心原则：Core 越小、越稳定越好；新增能力只有在多个平台领域真正共享且不需要高层依赖时才允许进入。

## 3. 公共值类型

### 3.1 FGamePlatformId（平台逻辑身份）

规范格式：

```text
namespace.name@version
```

规则：Namespace（命名空间）可点分，每段和 Name（名称）均为 ASCII 标识符；单段最长64字符，完整规范身份最长192字符；LogicalVersion（逻辑代次）为 1..INT32_MAX；规范输出统一小写；拒绝空白、符号、前导零、溢出、非 ASCII 和内嵌 NUL。

`TryParse` 与新增 `TryCreate` 失败都清空输出并保持无效状态；相等和 Hash（哈希）使用同一规范化语义。

它不是资产路径、`FPrimaryAssetId（主资产ID）`、网络主体身份或数据库主键。

### 3.2 FGamePlatformErrorCode（平台结构化错误码）

规范格式：

```text
domain.code
```

示例：

```text
data.asset.missing_definition
online.auth.token_expired
session.admission.ticket_expired
```

规则：

- Domain（错误域）允许点分层级，每段及 Code 均为 ASCII 标识符。
- 规范输出统一小写，完整文本最长192字符。
- 默认空值无效，采用 Fail Closed（失败关闭）。
- 支持规范相等、Hash、`TryParse`、`TryCreate`、`ToName`。
- 结构化错误码用于机器分支、日志聚合和遥测，不直接作为玩家 UI 文案。

兼容策略：

- 现有 `FGamePlatformResult.Code:FName` 不改变，避免破坏既有调用。
- `Failure(FGamePlatformErrorCode,...)` 与 `Unsupported(FGamePlatformErrorCode,...)` 作为渐进迁移入口。
- `TryGetStructuredCode` 只解析真正符合 `domain.code` 的代码；遗留裸码如 `LoadFailed` 返回 false，不猜测错误域。
- 各领域按插件逐步迁移，不进行一次性全项目错误码改名。

### 3.3 FGamePlatformVersion（平台三段版本）

格式为 `Major.Minor.Patch`，只表示三个非负整数，不携带预发布、构建元数据或兼容政策。

它与 FGamePlatformId.LogicalVersion（逻辑身份代次）、Plugin Version（插件发行版本）、Protocol Version（协议版本）、DataVersion（数据结构版本）和 Unreal Engine Version（虚幻引擎版本）必须区分。

### 3.4 FGamePlatformVersionRange（平台版本兼容区间）

表示闭区间 `[MinimumInclusive, MaximumInclusive]`。

- 默认 `bConfigured=false`，区间无效。
- 未配置区间的 `Contains` 始终返回 false。
- 上下界必须分别有效。
- Minimum > Maximum 时区间无效。
- 上下边界都包含。

它只提供“某版本是否落在某区间”的值判断，不决定客户端准入、协议兼容、内容更新或插件加载政策；这些政策必须由所属领域决定。

### 3.5 FGamePlatformResult（平台统一结果）

公共状态保持 `NotExecuted / Succeeded / Failed / Cancelled / Unsupported`。

- 默认 `NotExecuted`，不能误判为成功。
- 只有 `Succeeded + NAME_None Code` 才是成功。
- Message（说明）用于诊断，不是玩家本地化文案。
- Cancelled（取消）只描述终态，不代表事务补偿或远端回滚完成。
- 领域插件应在公共 Result 之外保留自己的领域错误枚举或结构；Core 不吸收几十种领域错误状态。

## 4. 错误码治理

推荐新代码使用稳定域，例如 `core.* / data.* / flow.* / online.* / session.* / world.* / gameplay.* / combat.* / presentation.* / arena.*`；实际可继续细分，例如 `data.asset.*`。

治理要求：

1. Code 是程序契约，不使用自然语言。
2. 已发布 Code 不因文案修改而改名。
3. Message 可以改进，但必须脱敏。
4. UI 不直接显示底层 Message，由上层映射本地化 Key（键）。
5. HTTP、数据库和第三方错误先映射到领域错误，再选择是否映射到平台结构化 Code。
6. 后续由 GamePlatformDeveloperTools（游戏平台开发者工具）逐步增加新代码命名与重复检查；遗留裸码通过迁移清单逐步收敛。

## 5. 线程、生命周期与所有权

所有公开类型都是自包含值，不持有 UObject（虚幻对象）、World（世界）、Player（玩家）、网络连接、回调或资源租约。独立值副本可以在线程间传递；同一可变实例的并发读写由调用者同步；作为 TMap/TSet（映射/集合）键后不得原地修改参与相等/哈希的字段。

模块启动只注册日志分类，不发网络、不加载地图、不创建玩家或进程级状态。

## 6. Blueprint/DataAsset（蓝图/数据资产）边界

`FGamePlatformId` 与 `FGamePlatformErrorCode` 保留可编辑反射字段，便于 Definition（定义）和工具链使用，但直接编辑不代表合法。

- C++ 构造优先使用 `TryParse / TryCreate`。
- DataAsset 编辑值必须在 GamePlatformData / GamePlatformDeveloperTools 的 Data Validation（数据校验）阶段重新检查。
- 不为了 BlueprintFunctionLibrary（蓝图函数库）给 Core 增加 Engine 依赖。
- 若后续确需 Blueprint 安全创建函数，由更高层工具或蓝图适配模块包装。

## 7. 3A工程实践要求

GamePlatformCore 必须保持 API 小而稳定、零项目知识、零网络与资源所有权、Fail Closed 默认值、可重复原生测试、Editor/Client/Server 三端编译、UHT反射验证、错误身份可聚合、版本值与兼容政策分离，并禁止因“方便”创建 CoreSubsystem / CoreManager。

## 8. 扩展决策

允许进入 Core 的新类型必须同时满足：至少两个平台领域确实需要；不依赖 Engine 高层、网络、资产或项目模块；属于稳定值契约而不是业务服务；可以独立测试；默认状态不会误报成功或兼容。

## 9. 当前验证状态

- Native Debug（原生调试版）：13/13 场景通过。
- Native Release（原生发行版）：13/13 场景通过。
- UE5.8 Editor 模块：UHT/编译/链接通过。
- UE5.8 Client 模块：UHT/编译通过。
- UE5.8 Server 模块：UHT/编译通过。
- UE Automation（虚幻自动化测试）：本轮未实际运行。
- Cook / Stage（烘焙/暂存）：本轮未作为独立发布门禁执行。

详细命令与证据见 [TestingAndEvidence.md](TestingAndEvidence.md)。