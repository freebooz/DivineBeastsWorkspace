# Application 与 PlayerServices 整改报告（2026-10-09）

工作区：`C:\Users\Freebooz\.codex\worktrees\gameplatform-review-fixes\DivineBeastsWorkspace`。分支：`codex/gameplatform-audit-fixes-20261009`。整合基线：`0f6afcb571185626487c4c5da3ee750a52b71bb0`。未提交、未推送；源码已冻结。

本轮把已存在的修复与新增修复分开，保留生产契约/资源及UE验证前提。没有编辑UI资产、两个Presentation模块或Data/Online实现。

| 审查ID | 当前源码/合同状态 | 未验证/条件 |
| --- | --- | --- |
| APP-01 | 当前ListPlayerMappings仅枚举ProfileMappingRows，PreviewRebind通过此集合验行，Apply消费Preview结果；Reset独占集合。旧修真实链已覆盖，不重复实现。 | InputPolicy Debug/Release与既有OwnedRowsReset源码；真实外部Context重绑尚待UE运行 |
| APP-02 | 普通客户端原生SaveSettings/INI Flush均void，明确只bPreferencesSaveSubmitted=true，bPreferencesSaved=false，不宣称落盘。PIE保存明确Unsupported。 | 真实磁盘失败终态仍无法从原生void接口确认，合同保留未知结果 |
| APP-03 | PIE命名空间加入LocalPlayer服务ScopeId；保存在调用原生SaveSettings/Flush前返回InputPIEPersistenceDisabled，内存偏好隔离。 | 同ControllerId双PIE、真实INI/原生存档隔离待UE场景 |
| APP-04 | ReadFile在分配前校验FileSize与打开句柄Size，并按GetMaxEncodedBytes限制实际读取；巨大/备份文件不先全读再拒绝。 | 真实超大文件/并发/慢盘/掉电回归待UE存储场景 |
| APP-05 | Loading分发回调返回后核对Scope存在、Id与Generation，再写计数/耗时；回调内GI关闭/新操作不会访问旧Scope。 | Loading原生Debug/Release通过；广播内真实GI关闭用例待UE |
| APP-06 | Inventory/Entitlement/Commerce在状态广播前持有共享Transport快照，广播后核账号/请求代次；Reset递归幂等，广播内Configure拒绝。商店全部六命令与ViewModel/订单后续状态都有保护。 | 新增ResetDuringState源码，UE未执行；生产HTTP取消/同步重入由根统一验证 |
| APP-07 | Equipment EndPlay/Destroy清理自有GAS并失效生命周期回调；本轮补每持久请求独立代次和终态消费资格。 | 生产PersistencePort不存在，现行Port无事务强杀/回滚接口；仅保证本地消费者失效和自有GAS撤销，ASC存活/退出迟到待UE |
| APP-08 | 共享完整/公开装备快照验证唯一槽/物品身份；server在撤销/Grant前预检整份快照和所有定义，visual创建前预检公开投影；复制组件也拒绝畸形快照。 | 新增重复槽位不撤销旧授予/不发布版本用例源码；真实GAS/网格、后端容量合同待UE与生产Port |
| APP-09 | Progression旧修OnViewChanged覆盖首次/轨道增删/错误/Reset/曲线；本轮补Online认证装配及请求代次隔离。兼容XP/等级事件继续只表达既有轨道差异。 | 现有Client事件Automation未执行；不以业务Tick替代事件 |
| APP-10 | LiveOps Reset发布通用/玩家清空和失效领奖事件，撤销账号Ticker；只在真实Initialize且有账号/Transport时登记UTC边界Ticker。 | SameRevisionView/TimeBoundary源码存在，账号A到B失败期间真实UI清空待UE |
| APP-11 | UI Retry已调用RetryFailedFlow，受理仅走已有取消/清空票据/Online注销后人工登录；无活动运行、不忙、装配完整且非创建结果未知时才公布Retry。 | 真实失败到新run/注销错误路径/非幂等创建不重发待UE流程资产；默认InputProfile缺真实资产诊断保留 |
| APP-12 | entitlements/tracks必须为实际数组；缺失/错类型拒绝，合法空数组仍成功；revision和等级数值在转整数前校验有限/整数/安全范围。 | OnlineOwnership源码补有效时间版本但缺集合、错类型、合法空集合负例；生产传输接收容量由既有Online.MaxResponseBytes统一流式限制，后端正式路由/版本联调仍缺 |
| APP-13 | 私有Completion返回是否首次受理，adapter仅true才唤醒GT；重复/过期/取消完成不排额外唤醒。 | 新增256并发完成仅受理一次、终态/取消后false；核心Debug/Release通过，UE TaskGraph/实际队列成本未实测 |
| APP-14 | 新增BeginLoad；Client原生异步存在性区分不存在/错误并AsyncLoad，完整校验/迁移/解析后候选原子提交。bLoading终态事件；Reload失败保留旧快照，换账号先撤销旧User。拓扑/销毁/重复/旧代次丢弃，广播快照不可变且拓扑唤醒合并。 | 新增AsyncLoadGeneration与异步A/B读源码未运行；无生产SettingsProvider、真实慢盘/大档案/平台存档环境未测 |
| APP-15 | 本轮检查37个PlayerServices Public头文件责任，重点字段单位/空值/错误、命令参数/受理与终态/引用寿命，Build职责与六README；补公开Definition、资格Port、查询、时间估计与ViewModel合同。 | 人工审核只覆盖本轮公开合同重点与修改流程；Private/历史文档与全工程中文全量审核仍为存量，不声称整体合规 |

## 实际检查

四个现有入口各Debug/Release configure/build/CTest退出0；Flow核心新增256并发完成回归，Input最终增加同ControllerId作用域键用例后重建/CTest退出0。
Settings/Save/DBAInput/DBAFlow四专项退出0；Settings实际警告无生产Descriptor Provider。
12项静态基线红/当前绿；UE专有场景只新增/维护源码，未冒称运行。

首轮把CMake产物放在长Saved路径，实际因Windows FileTracker FTK1011退出1；改用独占短路径后成功。没有降低工具链版本或关闭模块。完整实际命令、退出码和日志见RepairReport.json的Commands及NativeCommands.json/ArchitectureCommands.json。

## 接口与兼容影响

- IGamePlatformSettingsPersistenceProvider新增virtual BeginLoad改变ABI，所有消费者需重编；默认适配旧Load仅有界内存/Server，Client真实用户Load返回SettingsAsyncLoadRequired。
- Settings Snapshot新增反射bLoading默认false；Reload/Switch返回仅受理，调用方通过事件bLoading=false/LastResult确认终态；原Profile序列化/槽名不变。
- Equipment Types新增两个纯值验证函数；拒绝重复槽/物品/身份畸形快照，不迁移已有类型/字段；空Slots保持合法，Slot.Revision=0兼容保留。
- Entitlement新增只读GetLastError；DBA新增RetryFailedFlow；私有FlowCore Completion由void返回bool，不改变平台公开UObject Completion签名。
- 四个领域LocalPlayer默认随Online装配；保留Configure自定义/测试入口，但Reset广播内同步Configure现在拒绝。
- PIE输入真实保存现在明确Unsupported；普通客户端仍只公布受理。

## 中文审核范围与存量

本轮Public字段/命令/错误、文件责任与主要修改流程、测试前提/失败意义、Build和README；明确网络权威/账号和请求代次、货币最小单位/XP整数/UTC/秒/槽位范围、借用引用寿命、取消不回滚事务。
未进行所有Private函数、历史材料、未改模块与全工作空间逐文件中文全量审计；自动扫描不作为人工审核替代。

## 未执行与生产条件

- 本分工未运行任何UBT/UHT/UE Automation；root冻结后统一执行
- Editor/Client/Server实际模块构建及UE运行，由root统一账本记录
- 双PIE/多本地玩家、ASC存活/网格孤儿清理、真实广播退出与HTTP取消
- 真实InputProfile资产、Settings生产Descriptor、Equipment生产持久化/GAS解析、四领域后端路由/认证联调、真实支付Provider/经济闭环
- 干净Cook/Stage、服务器包表现剥离、联机、慢盘与CPU/GPU/内存实测、Monolith资产运行/人工视觉验收

## 修改/新增文件

仅列分配范围当前diff；根并行对同一Build.cs的目标/依赖补正需按完整diff确认作者边界。
- `Game/Plugins/DivineBeasts/DBAClient/README.md`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsApplicationFlowClient/Private/DivineBeastsApplicationFlowSubsystem.cpp`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsApplicationFlowClient/Public/DivineBeastsApplicationFlowSubsystem.h`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Adapters/Application/DivineBeastsApplicationUIAdapter.cpp`
- `Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/README.md`
- `Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/Source/GamePlatformApplicationFlow/Private/Execution/ApplicationFlowExecutor.cpp`
- `Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/Source/GamePlatformApplicationFlow/Private/Execution/ApplicationFlowExecutor.h`
- `Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/Source/GamePlatformApplicationFlow/Private/Subsystems/GamePlatformApplicationFlowSubsystem.cpp`
- `Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/Source/GamePlatformApplicationFlow/Private/Tests/ApplicationFlowExecutorCases.h`
- `Game/Plugins/GamePlatform/Application/GamePlatformInput/README.md`
- `Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp`
- `Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Tests/InputPolicyTests.cpp`
- `Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Public/Interfaces/IGamePlatformInputService.h`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Docs/Migration.md`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Docs/PerformanceAndSecurity.md`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Docs/SettingsLifecycle.md`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsClient/Private/Persistence/GamePlatformSettingsClientPersistenceProvider.cpp`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsClient/Private/Persistence/GamePlatformSettingsClientPersistenceProvider.h`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsClient/Private/Tests/GamePlatformSettingsPersistenceScopeTests.cpp`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsRuntime/Private/Subsystems/GamePlatformSettingsSubsystem.cpp`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsRuntime/Public/Interfaces/IGamePlatformSettingsProvider.h`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsRuntime/Public/Interfaces/IGamePlatformSettingsService.h`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsRuntime/Public/Subsystems/GamePlatformSettingsSubsystem.h`
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsRuntime/Public/Types/GamePlatformSettingTypes.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/README.md`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Services/GamePlatformCommerceClientSubsystem.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Tests/GamePlatformCommerceClientTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Transport/GamePlatformCommerceGatewayHttpTransport.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Public/Services/GamePlatformCommerceClientSubsystem.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Public/Transport/GamePlatformCommerceGatewayHttpTransport.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Public/Types/GamePlatformCommerceUITypes.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Public/ViewModels/GamePlatformCommerceViewModel.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/README.md`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlement/GamePlatformEntitlement.Build.cs`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlement/Public/Definitions/GamePlatformEntitlementDefinition.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlement/Public/Queries/GamePlatformEntitlementQuery.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Private/Services/GamePlatformEntitlementClientSubsystem.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Private/Tests/GamePlatformEntitlementClientTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Private/Tests/GamePlatformEntitlementOnlineTransportTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Private/Transport/GamePlatformEntitlementGatewayHttpTransport.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Public/Services/GamePlatformEntitlementClientSubsystem.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Public/Types/GamePlatformEntitlementClientTypes.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/README.md`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipment/GamePlatformEquipment.Build.cs`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipment/Private/Components/GamePlatformEquipmentComponent.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipment/Public/Components/GamePlatformEquipmentComponent.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipment/Public/Definitions/GamePlatformEquipmentDefinition.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipment/Public/Types/GamePlatformEquipmentTypes.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentClient/GamePlatformEquipmentClient.Build.cs`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentClient/Private/Components/GamePlatformEquipmentVisualComponent.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentClient/Public/Components/GamePlatformEquipmentVisualComponent.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentClient/Public/Definitions/GamePlatformEquipmentVisualDefinition.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/GamePlatformEquipmentServer.Build.cs`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Private/Components/GamePlatformEquipmentServerComponent.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Private/Gameplay/GamePlatformEquipmentGASGrantPort.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Private/Tests/GamePlatformEquipmentLifecycleTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Components/GamePlatformEquipmentServerComponent.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Gameplay/GamePlatformEquipmentGASGrantPort.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Interfaces/GamePlatformEquipmentPersistencePort.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/README.md`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/GamePlatformInventoryClient.Build.cs`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Private/Services/GamePlatformInventoryClientSubsystem.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Private/Tests/GamePlatformInventoryClientTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Public/Definitions/GamePlatformInventoryItemDefinition.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Public/Interfaces/GamePlatformInventoryClientTransport.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Public/Services/GamePlatformInventoryClientSubsystem.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Public/Types/GamePlatformInventoryTypes.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/README.md`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Private/Services/GamePlatformLiveOpsClientSubsystem.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Private/Tests/GamePlatformLiveOpsClientTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Private/Transport/GamePlatformLiveOpsGatewayHttpTransport.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Public/Services/GamePlatformLiveOpsClientSubsystem.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Public/Time/GamePlatformLiveOpsServerTimeEstimator.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Public/Transport/GamePlatformLiveOpsGatewayHttpTransport.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Public/Types/GamePlatformLiveOpsTypes.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/README.md`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgression/GamePlatformProgression.Build.cs`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgression/Public/Definitions/GamePlatformProgressionTrackDefinition.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgression/Public/Interfaces/GamePlatformProgressionSnapshotProvider.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgression/Public/Types/GamePlatformProgressionTypes.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Services/GamePlatformProgressionClientSubsystem.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Tests/GamePlatformProgressionClientTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Tests/GamePlatformProgressionOnlineTransportTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Transport/GamePlatformProgressionGatewayHttpTransport.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Public/Services/GamePlatformProgressionClientSubsystem.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Public/Transport/GamePlatformProgressionGatewayHttpTransport.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Public/Types/GamePlatformProgressionClientTypes.h`

新增源码需root同步总体目录规划：
- `Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsRuntime/Private/Tests/GamePlatformSettingsAsyncLoadTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Tests/GamePlatformCommerceJsonValidationTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Tests/JsonIntegerPolicyNativeTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Transport/JsonIntegerPolicy.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Tests/CMakeLists.txt`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Private/Tests/GamePlatformLiveOpsJsonValidationTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Private/Tests/JsonIntegerPolicyNativeTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Private/Transport/JsonIntegerPolicy.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Tests/CMakeLists.txt`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Tests/GamePlatformProgressionJsonValidationTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Tests/JsonIntegerPolicyNativeTests.cpp`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Transport/JsonIntegerPolicy.h`
- `Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Tests/CMakeLists.txt`

## 独立复核Important追加修复

| 项目 | 实现 | 验证边界 |
| --- | --- | --- |
| 拓扑通知即撤回旧Load代次 | 拓扑通知即撤回旧Load代次；Rebuild失败也不得发布旧候选，并以事件通知终态；未保存用户修改/Apply等待期间延后重载。 | AsyncLoadGeneration补注册表冲突后旧Completion不发布；UE未执行 |
| BeginSave前登记独立SaveRequestGeneration/实例代次/在飞 | BeginSave前登记独立SaveRequestGeneration/实例代次/在飞；同步/重复/迟到/启动失败共用完成一次门闩，Provider与值副本保活。 | SaveTerminalGate覆盖同步成功、旧重复、新保存资格、启动失败、迟到成功不得清Dirty；UE未执行 |
| Settings账号投影广播后复查 | Settings账号投影广播后复查；五领域bDeinitializing/InstanceGeneration，Configure的Reset通知关闭后不得复活，公开命令/回调拒绝关闭作用域，取消持活Transport。 | 五领域CloseDuringConfigure实际Subsystem/测试Port及Settings投影关闭；UE未执行 |
| Mutation/Query在外部Begin前设状态与独立终态身份 | Mutation/Query在外部Begin前设状态与独立终态身份；持活Transport并捕获Pending副本、账号与OperationId；快照回调在飞门闩拒绝重复。 | SynchronousOutcome及Reset/Close/既有Mutation用例；UE未执行 |
| Grant/Revoke候选与旧句柄脱离成员，Port/Resolver/Definition保活 | Grant/Revoke候选与旧句柄脱离成员，Port/Resolver/Definition保活；解析/GAS每步后检查寿命/Avatar，关闭撤销局部NewHandles，不提交孤儿能力；持久Begin也持活参数。 | ComponentRemoval扩展Resolver同步DestroyComponent，ASC仍存活且无授权；UE未执行 |
| Commerce金额/Quantity与LiveOps版本/Revision/priority/计数严格整数范围 | Commerce金额/Quantity与LiveOps版本/Revision/priority/计数严格整数范围；UE numeric-string DOM保留原number token，int64全范围精确解析；普通double只接受2^53−1精确范围；Progression字符串严格溢出。 | 三个生产私有纯值策略各Debug/Release build/CTest退出0；真实JsonValidation调用生产解析器源码未执行 |
| Commerce三集合、LiveOps目录/玩家/嵌套集合明确提供错类型即拒绝 | Commerce三集合、LiveOps目录/玩家/嵌套集合明确提供错类型即拒绝；对象/标签已知元素类型拒绝畸形。缺字段维持既有可省略合同，不造required schema。 | 真实JSON用例覆盖offers:{}、campaign_states:"bad"、目录非对象元素、合法省略与空/零值；UE未执行 |
| 目录/玩家状态/领取/领取查询各自独立代次门闩，Claim返回OperationId必须匹配捕获Guid | 目录/玩家状态/领取/领取查询各自独立代次门闩，Claim返回OperationId必须匹配捕获Guid；过期/重复不能消耗新资格。 | RequestTerminalGate覆盖同账号旧目录和错/重复ClaimOperationId；UE未执行 |

追加三个本模块私有整数策略入口，共6次Debug/Release CTest；15条configure/build/CTest命令全部退出0。纯值策略由实际生产JSON转换入口调用；不等价UE DOM/HTTP/生命周期运行。

追加UE回归已按真实入口写入，但本分工禁止UBT，未执行这些用例的修复前红灯和修复后绿灯；不能冒称已完成UE行为验证。已有前轮12静态红/绿只证明必要源码条件。

人工抽检追加的实例关闭/账号重入、保存/读取候选、装备GAS授权撤销、十进制算法与JSON适配、实际回归目的/前置/失败意义，以及六README和Settings Lifecycle。没有逐文件全量审核历史Private或全工作区。

新增Grant默认作用域探针改变导出符号，所有消费者必须重编译。JSON线格式和反射稳定身份不迁移。源码再次冻结，未提交或推送。完整新增文件清单已包含三个局部CMake、三份私有策略、三份原生测试、三个UE JSON测试和Settings异步测试；由root同步总体目录规划。

## 两处剩余Important定点收尾

- ReloadInternal统一发布Rebuild/ResolvePersistenceProvider准备失败终态，直接Reload与延后Topology唤醒均事件驱动结束Loading；外部调用后核实例/读取代次，caller移除重复Publish。 PreparationFailureEvent观察订阅者非Loading失败、直接/拓扑只一次；DeferredPreparationEvent真实GT延后失败事件。 Settings专项门禁退出0，UE用例未运行，修前/后UE红绿未执行。
- 内部Mutation/Query处理器接收原请求代次；OperationNotFound通知后监听器查询B接管则旧A停止；RevisionConflict监听器Snapshot接管则旧栈不再次刷新，不伪报BackendUnavailable。 QueryListenerTakeover/ConflictListenerTakeover实际OnChanged及Transport Begin计数，覆盖后续终态可完成。 范围diff与12必要静态条件退出0，UE用例未运行。

抽检本轮Settings准备失败事件所有权、外部Provider代次复核、Inventory广播接管资格、四个实际入口回归目的/前置/失败意义；更新Settings Lifecycle与Inventory README。历史中文全量审核仍未完成。

保留根修无Provider默认候选代次和Equipment TObjectPtr.Get修复。本轮仅上述七个已存在文件，没有新增受版本管理文件，没有修改根规划、生成物、资产或其他组源码。源码再次冻结；未提交、未推送、未运行UBT/UHT/UE Automation。

## 实际UE服务器编译失败的定点修复

根实际UE5.8 Server构建退出6，日志为`C:\Users\Freebooz\.codex\worktrees\gameplatform-review-fixes\DivineBeastsWorkspace\Saved\Validation\FoundationM0\d7a0e3ef-993d-47c3-a033-019a9b6e47bc\Build\Server\UBT.log`。本域GamePlatformEquipmentServerComponent.cpp第248/312行的WeakThis lambda缺少Self限定，导致C3493/C2327/C2065；仅将两处改为Self->bApplyingRuntimeSnapshot，保持弱UObject所有权和原请求门闩。同文件四个弱引用持久化回调及相关标记访问已抽检。范围diff检查退出0；本分工没有重跑UBT，修后编译未验证。源码再次冻结，未提交/推送。
