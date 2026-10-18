# SSLM GAS 角色技能授权重入安全与视觉策略整改记录（2026-10-18）

工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`，引擎：Unreal Engine 5.8（虚幻引擎）。展示名称统一使用 SSLM；现有模块、反射名、资产身份保持不变。

## 一、执行范围与所有权

本轮沿用三层架构及既有唯一 GAS（玩法能力系统）：
- 平台：`GamePlatformAbilitySystem`（能力系统）、`GamePlatformCombat`（战斗与属性）及`GamePlatformData`（定义及租约）。
- 项目服务端与双端玩法：`DBAGameplay/DivineBeastsAbilitiesRuntime`（角色技能装配和具体技能），角色拥有唯一 ASC（能力系统组件），`DivineBeastsCharactersRuntime`（角色身份、气势和资格）。
- 项目纯客户端：`DBAClient/DivineBeastsPresentationClient`（表现）与`DivineBeastsUIClient`（技能栏）。
- 不改12生肖正式技能审批、现有`_placeholder@1`占位集合、游戏伤害公式、GameplayEffect资产、地图、蓝图及Server Cook，且不启用已经取消的五行、克制、共鸣玩法。

工作区有其他会话的未提交项目表现、界面和技能射线源码修改，全部保留；本任务只编辑下列已确认的文件并新增隔离测试，不清理不提交不重启编辑器。

## 二、视觉策略静态测试修复（实测通过）

旧`Tests/Architecture/ValidateZodiacAbilityIntegration.py`（生肖技能集成验证器）误以为Village地图路径、生肖技能后缀和许可条件仍写在`DivineBeastsDevelopmentMagicPresentation.cpp`（客户端表现桥），导致源码已迁至`Development/DivineBeastsPlaceholderMagicVisualPolicy.h`（客户端白名单策略）后2项假失败。

现依据真实调用链验证：
1. 表现桥经`IsVillageWorld`只识别正式`L_Village_Start`及合法数字PIE后缀，拒绝专用服务器执行Niagara（粒子表现）。
2. `IsApprovedPlaceholderSpell`（占位技能白名单）只接受12生肖明确身份、三种法术类型及精确`_placeholder_...@1`后缀，不开放前缀匹配、其他技能和世界。
3. 开发魔法仍需双重显式许可；Village占位仅豁免**客户端视觉展示**，绝不授予技能、绕过Server GAS成本/冷却或冒充生产许可。
4. 另一并发会话对射线变量从`Hit`改为`Hit*`并补Pawn对象射线，静态检查现在接受对应语义真实调用，而不写入或覆盖其修改。

新增`Tests/Architecture/TestPlaceholderMagicVisualPolicyContract.py`（视觉准入故障注入）11项：合法链路1项及错误地图、非数字PIE、生肖身份缺项、宽松前缀、法术类别扩大、专服展示、移除Village限定、开发许可和技能ID校验、服务器表现风格缺失等反例10项。故障注入只在内存文本中进行，不编辑原生资产。

## 三、已落地的 GAS 授权事务同步重入修复

文件：`Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Components/DivineBeastsAbilityLoadoutComponent.cpp`。

### 发现的实际风险

原`RevokeOwnedGrants`（撤销自有技能）在循环调用`ASC->CancelAbilityHandle`、`ASC->ClearAbility`、`ASC->RemoveActiveGameplayEffect`、`Data->ReleaseDefinition`（外部可同步触发通知）之后，才清理旧`OwnedAbilityHandles`、`OwnedEffectHandles`、`DefinitionLease`和`PendingSetId`。一旦这些同步回调导致角色重绑和新技能请求，外层旧撤销栈可能把新请求的资源／句柄误删；刚完成的`ApplyGameplayEffectToSelf`或`GiveAbility`在返回句柄之前发生重入，也可能留下没入账的技能/效果。

这是源码可定位的生命周期风险，不代表已在真服务器复现。

### 本次修复

1. `RevokeOwnedGrants`先通过`MoveTemp`（移动）取出旧句柄数组，保存旧签发租约，并在调用任何 GAS/Data 外部入口前清空本组件当前所有权；只删除本次旧真实句柄及旧租约。
2. `AuthorityRefreshFromCharacter`（可信角色技能刷新）记录请求`RequestSerial`（代次）、英雄和化身身份；撤销旧授权、广播旧Ready=false后再次核对资格，旧同步调用栈不能覆盖后继角色／技能集合。
3. `HandleAbilitySetLoaded`（定义异步回调）核对租约的`ScopeId/LeaseId/Generation/IssuerProof/DefinitionId/ResourcePaths/Bundles`（作用域、唯一号、代次、签发证明、定义、资源闭包、加载分组）。失败时先确认旧请求仍有资格再撤销；撤销可能触发新请求，发布旧失败状态前再次核对请求身份。
4. `ApplyAbilitySet`（原生授予）在应用启动GE与`GiveAbility`前后重验请求代次、Hero、Avatar、ASC及当前租约；如果新效果或新Spec尚未入账时旧请求已失效，按刚返回的句柄就地撤销，防止遗留。
5. 原有非可撤销启动效果、动态属性集拒绝、费用/冷却数值一致性、ServerOnly（仅服务端执行）的占位技能策略及OwnerOnly（仅拥有者）显示快照保持不变。

新增`Tests/Architecture/TestGASLoadoutTransactionSafety.py`（GAS事务静态故障注入）7项：正常合同及缺请求版本、缺签发证明、缺失败回调代次保护、缺新GE/新Spec回滚、旧资源未预先摘除等反例。仅验证源码合同，不能模拟真实GAS委托。

## 四、当前实际验证与发布门禁

- `ValidateZodiacAbilityIntegration.py`：**49/49** 静态合同通过（已跟进原生技能命中调用形式与租约快照改造）。
- `TestPlaceholderMagicVisualPolicyContract.py`：**11/11** 测试通过。
- `TestGASLoadoutTransactionSafety.py`：**7/7** 测试通过。
- `ValidateCompactGASAttributes.py`：属性模型静态门禁通过，仍为2个具体属性集、10项GAS字段。
- 新增脚本Python语法检查通过；`git diff --check`针对本轮路径通过。
- GAS发布门禁`ValidateZodiacAbilityDelivery.py --release`在上一轮仍报告**70项未达到正式审批或原生联机/构建/Cook的发布要求**；这不是本轮源码安全修复能自动消除的缺项。

### 尚未完成/不能冒充通过

当前锁定UE5.8编辑器被其他会话使用，且HWebCodex报告并行活跃执行任务，因此本轮不触发可能覆盖DLL或抢占编辑器的UHT/UBT原生编译、Monolith蓝图制作或重新Cook。**上述C++修改仍需在独占Editor目标增量编译并通过UE原生同步委托重入测试后才可视为运行验证完成。**

必要的原生执行门禁：
1. 构建`DivineBeastsArenaEditor`（编辑器）当前全部受影响依赖，核对UHT/UBT日志及DLL版本；再做专服目标必要的增量构建。
2. UE真实测试：在GAS`ApplyGameplayEffectToSelf`、`GiveAbility`、`CancelAbilityHandle`和`RemoveActiveGameplayEffect`同步回调内模拟旧Pawn取消、新Hero角色身份切换、旧租约迟到，确认后继`OwnedAbilityHandles`和租约不被误删；检查非当前回调不能发布旧Ready。
3. Village Dedicated Server＋2客户端：出生加载占位技能→OwnerOnly槽位→按键输入→合法目标锁定→Commit后冰箭／火球／奥术伤害/治疗→死亡取消及重新进入→气势/能量与HUD独立值→重连。MainArena再执行对应竞技链路。
4. 专用服务器与客户端新版本Cook/Stage/Pak审查，禁止仅凭配置文本判断纯UI/Niagara剥离成功。
5. 正式12生肖技能设计及对应定义、游戏效果、动画、UI、美术和真实生产联机证据均须另行完成。不能将217个占位资产文件或60个GA蓝图直接计作已完成正式技能。

## 五、后续整改优先级

P0：运行时验证本次GAS技能撤销/授予同步重入安全；确认完整真实角色出生与换化身授权、占位法术连发中断及服务器复制。

P1：按能力种类制定服务器执行或客户端预测策略，测试预测失败时的取消、冷却和表现去重；补齐真实已授权技能的冷却动态显示与手机输入。

P1：继续用现有`GamePlatformCombat`处理技能效果、伤害、治疗、护盾及控制，不新增同义属性和并行战斗系统。

P2：正式生肖技能取代临时占位后，再依据真实功耗决定是否优化技能缓冲和VFX Budget（特效预算）。不得先按形式为每个技能新建平台插件。

## 六、2026-10-18 续行：NotReady 重入与真实 GAS 引擎回归代码

继续复核前次事务修复发现：`HandleCharacterReadinessChanged(false)`（旧角色失去就绪资格）虽然先执行 `++RequestSerial` 和 `RevokeOwnedGrants`（撤销自有授权），却在撤销引发的同步 GE 事件返回后**无条件**执行 `PublishLoadout(NAME_None, 0, false, {})`（广播空技能栏）。若该回调期间后继角色已取得新的 Ready 身份，旧事件会覆盖后继技能栏。这是原生源码可确认的执行顺序风险。

本轮对该分支增加原请求序号和权威 Owner 身份复核；只允许当前序号的 NotReady 事件发布空快照，已被后继请求接管的旧栈立即退出，不改其他角色 GAS 授权和已批准资源。

新增 UE5.8 原生测试文件：
`Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Tests/DivineBeastsAbilityLoadoutReentryTests.cpp`（真实技能装配重入自动化）。

- `DivineBeasts.Abilities.GrantTransaction.NativeRemovedEffectPreservesSuccessor`：使用真实 `UGamePlatformAbilitySystemComponent`（平台 ASC）和实际 GameplayEffect（玩法效果）句柄；旧效果 `RemoveActiveGameplayEffect`（移除主动效果）发出 `OnAnyGameplayEffectRemovedDelegate`（原生移除通知）时同步生成后继效果，验证旧撤销只释放旧效果，新效果及后继 `PendingSetId` 保留。测试中的平台护盾 GE 仅作为可观察的有期效果夹具，并非把带 GEComponent 的护盾效果加入正式 AbilitySet 授予白名单。
- `DivineBeasts.Abilities.GrantTransaction.NativeReadinessCallbackCannotClearSuccessor`：同样使用真实 GAS 移除通知，在旧 NotReady 清理回调中模拟后继请求代次及 Ready 快照，验证旧方法返回后后继英雄身份、Ready 与唯一 Revision（修订）未被覆盖。后继状态是隔离测试夹具，不冒充真实服务器授权。

测试仅在 `WITH_DEV_AUTOMATION_TESTS`（开发自动化测试编译开关）生效时通过友元访问私有账本；正式 Shipping（发布构建）不增加公开写接口、不放宽 ASC 或角色认证。真实引擎运行须等待与新源码一致的编译模块；目前尚未执行。

相应扩充原静态 `TestGASLoadoutTransactionSafety.py`（技能授权事务门禁）：从 7 项扩展为 **9 项**，新增旧 NotReady 不得覆盖后继、原生测试必须真实订阅 GAS 移除委托而非只用文本模拟；内存故障注入仍不写资产。

### 续行本轮静态结果与限制

- `TestGASLoadoutTransactionSafety.py`：9/9 通过；
- `ValidateZodiacAbilityIntegration.py`：49/49 通过；
- `git diff --check`：受影响路径退出0；
- HWebCodex 查询已有一个运行中的 `UnrealEditor.exe`（虚幻编辑器）进程，其源码模块与当前未提交改动不能视为匹配；本轮没有抢占、重启、热加载或覆盖该进程。
- **两项新 Native（原生）测试现在只是已写入源码，未经过本轮 C++ 编译或 UE 测试实际执行；不能标记为通过。**
- 正式十二生肖生产技能交付与双客户端/专用服务器 Cook（资源烘焙）仍沿用上节的未完成门禁，不以本次事务安全改动冲抵缺失项。

下一次受控原生门禁先编译当前 `DivineBeastsAbilitiesRuntime`（技能运行模块）及受影响依赖，再要求两项 `GrantTransaction.Native*` 在自动化测试列表真实出现且均通过，并检查取消后继资源不被错误撤销，最后执行 Village/竞技双端联机链路。

## 七、2026-10-18 原生编译器独立验证（新增证据）

在上一轮两项新增原生自动化测试尚不能随共享UE编辑器完成UBT重链接时，本轮不抢占现有UE5.8编辑器，不运行Live Coding（实时编译），不覆盖任何`Binaries/Win64`（插件二进制目录）或正式`Intermediate`（构建缓存）生成物，采用只读取当前模块已生成`Definitions.h`（宏定义头）及`DivineBeastsAbilitiesRuntime.Shared.rsp`（模块编译参数）的隔离 MSVC 检查：

| 目标翻译单元 | `cl /Zs`（仅语法） | `cl /c`（生成隔离目标文件） | 文件 |
|---|---|---|---|
| `DivineBeastsAbilityLoadoutComponent.cpp`（GAS技能装配组件） | 退出0 | 退出0 | `Saved/Validation/GASLoadoutIsolatedObjects_20261018/AbilityLoadout.obj` |
| `DivineBeastsAbilityLoadoutReentryTests.cpp`（两项真实GAS委托重入测试） | 退出0 | 退出0 | `Saved/Validation/GASLoadoutIsolatedObjects_20261018/LoadoutReentryNativeTests.obj` |

- 编译器：已安装`MSVC 14.44.35207`（微软 C++ 编译器）；目标：Win64、C++20、UE5.8 Editor Development 的现有实际模块参数；当前`Definitions.h`包含`WITH_DEV_AUTOMATION_TESTS=1`（开发自动化测试开启）。
- 两份隔离对象大小：13,268,919字节、11,307,932字节；均由本轮真实`cl.exe`执行生成。编译器只提示 UE 自身`StatsSystemTypes.h`（引擎统计字段）枚举组合的`C5054`弃用警告，不含当前目标源文件的编译错误。
- 对象保留在被忽略的`Saved/Validation`（本地验证目录）中，仅用于本轮证据，**没有链接成可加载UE DLL、没有替换正式二进制，也没有运行自动化游戏测试**。
- 工程仍有并行活跃Job和UE Editor进程；遵守当前构建锁定策略，不能为完成测试而抢占插件DLL、停止编辑器、重建UE或伪造PIE（编辑器运行）/Dedicated Server（专用服务器）运行结果。

因此这两项原生用例已从“只有未经编译源码”推进为“UE真实MSVC目标文件编译通过”，但仍要等待最新UHT/UBT完整Editor目标构建及实际UE `Automation RunTests DivineBeasts.Abilities.GrantTransaction.Native`（原生自动化测试）执行，才能判断GAS回调重入是否真正按预期安全。

## 八、2026-10-18 继续验证：原生 C++ 对象编译与独立授权账本执行

本轮确认共享 UE5.8 Editor 仍在运行、HWebCodex 工作区仍报告其他3项活跃Job；未抢占 Editor、未触发 Live Coding、未链接/替换 `UnrealEditor-DivineBeastsAbilitiesRuntime.dll`。为补强原来只有静态检查的证据，在独立临时目录执行了实际 MSVC 编译：

1. 使用现有 `DivineBeastsAbilitiesRuntime.Shared.rsp`（模块共享编译参数）和 `Definitions.h`（现有UE5.8编辑器生成定义）从引擎源目录执行 `cl /Zs`（语法检查）。`DivineBeastsAbilityLoadoutComponent.cpp`（技能装配实现）和 `DivineBeastsAbilityLoadoutReentryTests.cpp`（新原生测试）均退出0。
2. 同样参数对这两个真实源文件执行 `cl /c`（生成机器目标代码、**不链接**），均退出0，写入彼此独立的 `Saved/Validation/GASLoadoutIsolatedObjects_20261018/AbilityLoadout.obj`（13,268,919字节）和 `LoadoutReentryNativeTests.obj`（11,307,932字节）。输出路径不属于共享 `Intermediate/Binaries`，没有改变运行中的旧DLL。编译只出现UE引擎统计头文件的旧C5054枚举警告，当前新增源文件无编译错误。
3. 平台已有的 `GamePlatformAbilitySystem/Private/Tests/AbilityOwnershipTests.cpp`（授权账本独立测试）使用同一 MSVC 工具链及 `/utf-8`（UTF-8中文编码）编译为临时程序并实际运行，退出0，`Assertions=27 Failed=0`（27项断言、0失败），覆盖重复来源、旧代次、撤销迟到、独立账本和输入所有权。该测试使用的是 `FGrantLedger`（独立C++授权策略表）；真实项目grep表明目前它只在策略头与这份测试中被引用，**不等价于游戏实际ASC授权运行**，不能替代新写的两项UE GAS本机重入测试。
4. 完整`DivineBeastsArenaEditor`（编辑器目标）的最新 UHT/UBT（反射处理／统一构建）、插件链接、在新DLL上执行 `DivineBeasts.Abilities.GrantTransaction.Native*`（真实引擎自动化）、Village/竞技双客户端专服和客户端/服务器Cook，**本轮仍未执行**。旧模块二进制时间戳比当前技能源码修改更早，不能把运行中的编辑器视为新DLL测试通过。

本轮未修改正式 GAS 模块、蓝图或引擎；仅新增隔离构建证据与本实施记录。下一步在获得不与其他会话冲突的UE构建窗口时，优先进行最新Editor增量链接和两项已编译原生测试的真实运行。所有正式生肖技能发布与资源批准缺项仍待单独交付。

## 九、2026-10-18 续改：用户指定“修改代码、蓝图等，不编译”

本批用户明确禁止构建，因此从接到此指令后，**没有运行UHT、UBT、MSVC、Live Coding、Blueprint Compile、Cook/Stage**，更没有启动／重启UE Editor或更改现有Binaries、Intermediate。上一节的目标文件编译证据属于**此前批次**，不能误当本批执行。

### 已修改真实C++源码

- `Private/Components/DivineBeastsAbilityLoadoutComponent.cpp`（项目技能装配）及其公开头：新增 `bEndingPlay`（退出中）生命周期门禁，`EndPlay`在解绑委托和取消真实GAS句柄**之前**先使授权代次及新调用失效；`AuthorityRefreshFromCharacter`、`HandleCharacterReadinessChanged`、`HandleAbilitySetLoaded`、`ApplyAbilitySet`及 `PublishLoadout` 拒绝结束中的角色和旧世界异步数据。客户端 `OnRep_LoadoutState`（技能授权复制通知）也不在退出期复活旧 UI。
- `AcquireDefinition`（获取技能集定义）返回值改为局部 `NewLease`（数据租约）接收，复核请求代次、英雄及化身一致、`PendingSetId`仍属同一请求后才赋值给组件 `DefinitionLease`。过期或申请失败只释放自己的返回租约，禁止旧同步返回覆盖新请求的租约。
- `PublishLoadout`（技能栏状态发布）在事件广播后核对组件是否已退出和 `Revision`（状态修订号）是否被新事件推进，旧栈不能再继续发布过时的网络更新。
- `Private/Tests/DivineBeastsAbilityLoadoutReentryTests.cpp`（技能装配原生自动化）增加第三项真实GAS移除事件测试 `NativeEndPlayCallbackRejectsNewGrant`：退出期间旧 GE（玩法效果）移除委托尝试重新授予技能及发布新英雄状态，必须被拒绝。三个原生测试均属于**代码写入待验证**，本批没有编译或运行。
- `Tests/Architecture/TestGASLoadoutTransactionSafety.py`（纯静态失败关闭测试）加入退出门禁、异步租约安装顺序、旧状态发布终止和对应故障注入；测试是否通过以实际返回的测试日志为准，不以文件存在推断通过。

### 蓝图操作边界与待执行清单

当前HWebCodex工作区中的`UnrealEditor.exe`（UE5.8编辑器）和两份`monolith_proxy.exe`（Monolith代理）仍由并行会话使用，但**HWebCodex未注册可调用的Monolith MCP服务器**。另一个Cwebcodex Runner虽然登记`monolith`服务，`monolith_status`实际报`Unreal Editor not running`，且该Runner工作区与当前主工作树不是同一版本（存在Git冲突），绝不能使用它直接改写当前工作区资产。

因此本批**没有修改／保存任何真实Widget Blueprint（控件蓝图）或其他.uasset**，更没有绕过Monolith使用Python、二进制补丁或假资源。已生成独立、可追溯的 `GASBlueprintNoCompileReviewQueue_20261018.json`（GAS蓝图不编译审查队列），精确列举已存在的 `WBP_DBA_UI_AbilityBar`（技能栏）、`WBP_DBA_UI_AbilitySlot`（单技能槽）、`WBP_DBA_UI_CombatHUD`（战斗HUD）、`WBP_DBA_UI_PlayerStatus`（生命能量气势）和`WBP_DBA_UI_EnergyBar`（独立蓝色能量条），并登记控件名、归属、当前待审项和Monolith访问阻断。此JSON是**待执行任务记录**，不等于蓝图已经修改。

等拥有当前编辑器的Monolith MCP可用时，须先做Widget树、父类、实际绑定检查，必要时最小化修改并保存；用户未解除“不编译”限制前仍不得执行任何Blueprint编译。编译、冷重载、双端联机及Cook验收持续标记未完成，不得对外称全部通过。

## 十、2026-10-18 GAS技能集合及技能栏同步继续整改（保留“不编译”要求）

本批直接修改已有源码，不重建插件、不修改固定的十二生肖资产身份、PBR/VFX资源或正式技能策划数值；继续保持基础平台`GamePlatformAbilitySystem`（通用玩法能力系统）→项目双端`DivineBeastsAbilitiesRuntime`（项目技能）→客户端`DivineBeastsUIClient`（技能栏）的三层责任隔离。

### 已写入的服务端防护

源文件：`Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Components/DivineBeastsAbilityLoadoutComponent.cpp`（服务器技能授权组件）。

1. 在 `ApplyAbilitySet`（应用能力集合）授予任何技能前，对已获取 `UGamePlatformAbilitySetDefinition`（技能集合定义）的实际 `GetPrimaryAssetId`（主资产身份）与 `PendingSetId`（当前请求ID）进行精确匹配。平台DataService的签发租约不足以让同英雄的错误集合冒用此次身份。
2. 已有平台定义层通过软路径检查重复条目；项目层新增 `TSet<const UClass*> NewAbilityClasses`（解析后的类集合），拒绝不同重定向软路径最终落到同一个能力类。另拒绝已标为 `CLASS_Abstract`（抽象）、`CLASS_Deprecated`（已废弃）、`CLASS_NewerVersionExists`（由新热重载版本替代）的原生类。
3. 因 `UDivineBeastsConfiguredGameplayAbility`（配置技能基类）持有每次激活的Commit状态，拒绝 `NonInstanced`（不实例化）及 `LocalOnly`（仅本地）的技能配置；仍不将全部正式技能锁死为`ServerOnly`（仅服务器），保留将来经批准后的预测策略。
4. 成本 `GameplayEffect`（玩法效果）除已有同级别Momentum（气势）单一负值修正外，还拒绝 `Executions`（自定义执行计算）、非零 `Period`（周期）及未经审核的 `GameplayEffectComponent`（附加效果组件）。确保修改器数值恰好匹配不能掩盖其它扣费、治疗、伤害或授技副作用。
5. 冷却GE要求纯有限期标签，不允许通过 `Modifiers`（属性修改器）、`Executions`或非零Period暗中修改游戏数值。保留原UE5.8 TargetTags（目标标签）组件，以便正常提供冷却标签。其具体是否真的授予了GA所要求的标签，仍须编辑器原生资产和自动化测试复核；不以字段存在冒充实际GAS行为。

### 已写入的客户端同步修复

`DivineBeastsAbilityBarViewModel.cpp`（技能栏视图模型）刷新详情差异时增加对 `CooldownEndsAtAppSeconds`（冷却在应用时钟的截止秒）变化的检测：当同一技能原冷却被刷新、比例/剩余/总秒数恰巧几乎相同时，仍广播新的只读槽位状态，复用已有 `DivineBeastsAbilityBarPanel::RefreshVisualSlots`（技能栏视觉刷新）及 `ApplyOverlayTiming`（遮罩时钟写入），使已存在的材质冷却遮罩跟随新的效果截止时间，而无需额外Tick（逐帧轮询）。

### 代码与测试实绩

- 修改 `Tests/Architecture/TestGASLoadoutTransactionSafety.py`（静态事务负向回归）：新增主资产身份、解析后重复类、被弃用类、LocalOnly、成本执行计算、冷却额外修改器及实际C++括号/分支嵌套结构检查；从前一轮9项测试继续扩展。**本批仍未执行完整Python测试套件，不能标记所有用例通过。**
- 修改 `Tests/Architecture/ValidateZodiacAbilityIntegration.py`（生肖集成静态门禁）：增加冷却到期时钟重置、UClass和执行策略、成本与冷却隐式副作用共3项断言。当前源码预期门禁合计52项；**尚未取得本批Python脚本真实执行退出码**。
- 对当前 `HWebCodex`（开发环境）最新源文件直接做12项只读结构核对，12/12项匹配，涉及整个 `ApplyAbilitySet` 的括号配对、Configured安全分支、CooldownPeriod所在大括号范围、成本/冷却限制、实际资产ID、退出生命周期及技能栏时钟联动。**这只是源码快照检查，不是编译或UE游戏运行。**
- 由于HWebCodex存在并发排队执行任务，本批计划运行的Python脚本未能在当前可用槽位即时取得最终退出结果；有排队Job不能记为通过。
- **绝不运行**UHT、UBT、MSVC、Live Coding、蓝图编译、Cook、Editor新进程、联机测试，也不替换共享DLL。本批不影响上一节历史版本的隔离编译记录，它不代表本批新源码编译通过。

### 蓝图与原生资产真实状态

当前工程的真正Monolith MCP编辑器资产控制入口依旧未连接到本HWebCodex Runner；另一CWebCodex环境版本不同且存在Git冲突，不能越过所有权直接写入 `.uasset`（引擎二进制资产）。本批没有伪造修改或保存成功，`GASBlueprintNoCompileReviewQueue_20261018.json`仍作为独立跟踪清单。用户坚持“不编译”时，所有需经原生Monolith修改的蓝图及后来Blueprint编译验收均单独标记未完成。

最终交付门禁仍包含12生肖正式技能审批与资产回读、AbilitySet启动授权、真实冷却Tag、Village与竞技场双客户端专用服务器施法、UI视觉和客户端/专服资源剥离。不能把现有占位资产存在或本批纯源码修改当成整个GAS已可发布。





