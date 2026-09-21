# GamePlatformCore（平台核心契约）

本插件提供中立身份、三段数字版本、调用结果与日志分类。当前为任务01的源码交付：生产纯算法已通过原生测试，UE反射、目标构建与宿主集成尚未验证。插件没有内容资产、配置扫描、子系统、资源加载或服务注册，不连接后端。

## 模块、使用者与所有权

唯一模块为 `GamePlatformCore`，宿主类型 `Runtime`，加载阶段 `Default`，允许 Game／Client／Server／Editor 目标与命令行加载。唯一公开依赖是 UE 的 `Core` 和 `CoreUObject`；不依赖 Engine、Data、ApplicationFlow、MOBA 或项目层。模块启动只使用引擎默认模块注册机制，导出 `LogGamePlatformCore`。

预期调用者为平台 Data、ApplicationFlow 和薄宿主。消费方需在自身 `.uplugin` 启用本插件，并按公开签名使用情况在模块构建规则声明 `GamePlatformCore` 依赖。插件默认不自动启用；正式主工程启用、构建和全局目录文档由父任务统一处理。

三个结构均为自包含值，不持有 UObject、世界、异步请求或租约。独立值可在线程间复制；对同一实例的并发读写须由调用者同步。本插件不承诺网络传输、存档协议或业务授权。容器键插入后不能原地修改。

## 已锁定公开接口

公开头位于 `Source/GamePlatformCore/Public`。既有旧API不做迁移；实施前在 Game 内未找到同名身份、结果或版本结构。

| 头文件 | 类型与入口 |
| --- | --- |
| `Types/GamePlatformId.h` | `FGamePlatformId`；可编辑 `FString Namespace`、`FString Name`、`int32 LogicalVersion=1`；`IsValid()`、`ToString()`、静态 `TryParse(const FString&, FGamePlatformId&)`、`==`、`!=`、自由函数 `GetTypeHash(const FGamePlatformId&)` |
| `Types/GamePlatformVersion.h` | `FGamePlatformVersion`；可编辑 `int32 Major=0`、`Minor=0`、`Patch=0`；`IsValid()`、`ToString()`、静态 `TryParse(const FString&, FGamePlatformVersion&)`、`int32 Compare(const FGamePlatformVersion&)`、`==`、`!=` |
| `Types/GamePlatformResult.h` | `EGamePlatformResultStatus`：NotExecuted／Succeeded／Failed／Cancelled／Unsupported；`FGamePlatformResult` 含 `Status`、`FName Code`、`FString Message`；静态 `Success()`、`Failure(FName,FString)`、`Cancelled(FString)`、`Unsupported(FName,FString)`，以及 `IsSuccess()` |
| `GamePlatformCore.h` | 导出的 `LogGamePlatformCore` 日志分类；调用者负责信息脱敏 |

### 身份规则

完整格式为 `namespace.name@version`；最后一个点分隔 Namespace 与 Name。例如 `Platform.Content.Item_2@1` 规范化为 `platform.content.item_2@1`。

- Namespace 是一个或多个非空点分段；每段及 Name 均匹配 `[A-Za-z][A-Za-z0-9_]*`，各最多64个字符。
- Name 是单段；禁止借可编辑字段中的点注入另一层命名空间。
- 完整规范文本（包含点、`@` 和版本数字）最多192个ASCII字符。
- LogicalVersion 范围为1至2147483647；拒绝十进制前导零、正负号、空白、溢出和内嵌NUL。
- `TryParse` 成功输出ASCII小写；失败清空命名空间与名称，LogicalVersion恢复1，得到无效身份。输入可以引用输出结构的字符串字段。
- 可编辑大写值仍可有效，`ToString` 输出小写；非法值返回空串。每次检查、比较和哈希均重新读取字段，不依赖先前校验缓存。
- 有效身份按规范字段与逻辑版本比较；无效身份按原始字段精确比较，保持自反性，不把所有无效值合并，也不让无效值等同于有效身份。哈希使用相同规则。反射相等比较通过 `WithIdenticalViaEquality` 接入同一语义；其引擎执行仍待UE自动化验证。

逻辑身份不是 `FPrimaryAssetId`、对象路径或加载句柄；资源注册、版本接纳与加载所有权归 Data。哈希只是容器工具，不是密码学标识，也不作为序列化协议。

### 版本与结果规则

`FGamePlatformVersion` 是三个0至2147483647的数字，严格输入 `Major.Minor.Patch`。零版本有效；仅单独的数字 `0` 允许以零开头，不接受符号、空白、后缀、预发布标记或构建元数据。失败输出 `0.0.0`，因此必须检查 `TryParse` 的布尔返回值，不能仅凭输出有效就推断解析成功。比较按主／次／补丁顺序返回 -1、0、1，不通过减法造成溢出；对非法编辑值也能确定排序，但接纳前仍需检查有效性。

身份逻辑代次、三段数值版本、插件发行版本、协议版本和引擎版本是不同概念；本插件不制定自动兼容政策。

结果默认 `NotExecuted`，不属于成功。`Success()` 清空代码和说明；`Failure` 与 `Unsupported` 接受调用者诊断，缺少 Code（`NAME_None`）时分别补充 `MissingFailureCode` 与 `MissingUnsupportedCode` 并在说明中指出缺码，保留原始说明；已有代码但 Message 为空时补充中文说明。`Cancelled` 使用 `Cancelled` 代码，空说明得到中文取消诊断。只有 Succeeded 且 Code 为 None 才通过 `IsSuccess()`；取消结果不执行真实取消或事务补偿。蓝图只读结果字段；C++可直接赋值，因此调用者须保持字段一致。

## 最小调用示例

```cpp
#include "Types/GamePlatformId.h"
#include "Types/GamePlatformResult.h"

FGamePlatformId DefinitionId;
if (!FGamePlatformId::TryParse(TEXT("platform.content.example@1"), DefinitionId))
{
    return FGamePlatformResult::Failure(FName(TEXT("InvalidDefinitionId")), TEXT("定义身份格式不合法。"));
}
// 此处仅完成身份校验；由调用方把DefinitionId交给已接入的数据服务。
return FGamePlatformResult::Success();
```

此片段的成功只表示身份解析成功，不声称资源加载或宿主启动已经完成。三段版本使用同样的显式失败判断方式。

## 测试入口与验证边界

在工作空间根目录执行，要求 CMake、Visual Studio 2022 C++工具链及Windows SDK：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Game/Plugins/GameFoundation/Core/GamePlatformCore/Tests/RunNativeTests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Game/Plugins/GameFoundation/Core/GamePlatformCore/Tests/RunNativeTests.ps1 -Configuration Release
```

脚本从自身位置解析工作空间，不硬编码磁盘或用户名。默认日志、生成工程和二进制均进入工作空间 `Saved/Validation/FoundationM0/CoreNative`；`-BuildDirectory` 可指定其他目录。每个原生命令记录输出和退出码，失败立即停止并传播退出码。PowerShell脚本采用UTF-8 BOM以兼容Windows PowerShell 5.1中文解析；其他源码采用UTF-8。

原生测试直接包含生产私有算法，不使用UE替身或另一份解析器。CMake编译模块 `Private/Tests` 中的独立入口；此入口在UE构建中被宏排除。UE自动化用例同样位于 `Private/Tests`，由 `WITH_DEV_AUTOMATION_TESTS` 控制，过滤前缀 `GamePlatform.Core`，包含 Identity／Version／Result 三个测试，用于补验 FString/FName转换、反射字段、反射比较和容器查找。

2026-09-21 实测环境：CMake 4.3.2、VS2022生成器、MSVC 19.38.33145.0、Win64；Debug与Release各11个CTest场景通过，命令退出0。11个场景分别是身份正常／异常／长度边界／比较哈希／别名输入，版本正常／异常／排序，多字符宽度，以及结果状态／诊断。场景内包含整数上界、溢出、前导零、内嵌NUL、大小写归一、192字符边界、非法编辑值、失败清空和缺码诊断断言；不把内部断言数冒充独立测试数。

测试先于实现落盘。首次Debug构建因尚未存在 `GamePlatformCoreAlgorithms.h` 产生 C1083、退出1；结果测试加入后因尚未存在 `GamePlatformResultPolicy.h` 同样退出1。它们是缺少生产实现的编译失败，未声称已经观察到行为断言失败。首次9场景通过后的构建目录，经绝对路径、文件来源、无链接和无已跟踪文件检查，完整保留迁移到工作空间 `Saved/Validation/FoundationM0/CoreNativeFirstAttempt`；该缓存记录旧目录，只作首次尝试证据，不能原地复用构建。早期PowerShell转录遗漏原生命令输出，已经改为显式捕获并重新执行，最终采用包含CTest结果和退出码的日志。

UE源码只读核对基线为5.8.0、CL0。按父任务明确约束，保留现存空占位描述文件，**本轮未执行UBT、UHT、UE自动化、Client／Server／Editor构建、Cook、Stage或宿主运行**，没有新建宿主绕过扫描阻断。原生通过不能证明UE适配已编译、反射已注册或正式主工程可运行。

## 实际文件目录与交付范围

```text
GamePlatformCore/                         # 平台核心插件，唯一维护源码
├── GamePlatformCore.uplugin              # UE5.8单Runtime模块描述
├── README.md                             # 中文契约、接入、验证与目录
├── Source/                               # 源码根目录
│   └── GamePlatformCore/                 # 唯一模块
│       ├── GamePlatformCore.Build.cs     # Core与CoreUObject依赖
│       ├── Public/                       # 稳定消费边界
│       │   ├── GamePlatformCore.h        # 日志分类导出
│       │   └── Types/                    # 反射值类型
│       │       ├── GamePlatformId.h      # 中立逻辑身份
│       │       ├── GamePlatformResult.h  # 调用结果
│       │       └── GamePlatformVersion.h # 三段数字版本
│       └── Private/                      # 内部实现
│           ├── GamePlatformCoreModule.cpp # 默认模块与日志注册
│           ├── Parsing/                 # 原生与UE共用的生产策略
│           │   ├── GamePlatformCoreAlgorithms.h # 身份、版本、哈希算法
│           │   └── GamePlatformResultPolicy.h   # 结果状态和诊断策略
│           ├── Types/                   # UE值与私有算法的适配
│           │   ├── GamePlatformId.cpp    # 身份实现
│           │   ├── GamePlatformResult.cpp # 结果与UTF-8边界转换
│           │   └── GamePlatformVersion.cpp # 版本实现
│           └── Tests/                   # 实际编译入口，分别受宏控制
│               ├── GamePlatformCoreNativeTests.cpp # 原生行为测试
│               └── GamePlatformCoreAutomationTests.cpp # UE反射适配测试
└── Tests/                                # 外部构建与验证脚本
    ├── CMakeLists.txt                    # 原生编译和11场景注册
    └── RunNativeTests.ps1                # Saved输出与退出码传播
```

本轮仅新增此插件内文件，未创建 CoreSubsystem 或服务，未修改 Data／Flow 的公开签名、主工程、其他插件、Backend或Shared。全局目录与接口索引交父任务同步；本插件不操作Git提交或工作树。生成缓存、日志、二进制不属于上述源码交付清单。
