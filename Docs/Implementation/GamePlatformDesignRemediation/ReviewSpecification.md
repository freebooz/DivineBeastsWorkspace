# GamePlatform 设计审查整改规格

日期：2026-09-30。依据：39 插件、63 模块源码审查及用户“生成计划、执行修改、提交远程”的授权。执行基线为 `6f25a6519c9cc3850dd612bd073a3d79c0ba130a`，实施前逐条重核当前源码；已修正的问题不重复改动。

## 范围与验收原则

修复已确认的正确性、生命周期、依赖和合同缺陷，保留三层方向、稳定反射/协议/插件身份以及既有取消保护。F19 的尚未实施功能以真实成熟度、职责和后续立项门禁收口；F20 修正文档中的交付失真，不据此扩建支付/奖励/持久化业务。F16 的存量资源迁移保持真实消费者兼容，不能伪造定义资产、复制资产管理器或直接改变发布身份。

每项必须记录：重核结论、修改范围、回归用例、实际命令/退出码、未执行运行证据。源码门禁不充当UE动态测试；测试源必须在模块Private/Tests或已声明测试模块。正式源码中文说明随修改补齐。新文件由总体目录规划同变更登记。

## 发现与行为要求

### F01 P1：四个玩家服务HTTP适配器形成强引用循环

CommerceUI、Entitlement、Progression、LiveOps 的完成委托同时强捕获 RequestPtr 和 Self。请求本身拥有委托，完成时移出 ActiveRequests 不会解除 Request→Delegate→Request 循环，Transport及其Token也被保留。锁定UE5.8正常完成仅Execute委托、从管理器移除请求，并不调用Shutdown解绑。

整改要求：使用回调请求入参或弱请求引用，统一终态解绑；优先复用Online受保护请求通道。回归成功/启动失败/取消/退出后弱引用均失效。

核查入口：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Transport/GamePlatformCommerceGatewayHttpTransport.cpp`。原审查边界：源码及锁定HTTP实现确认；实际内存增长未测量。

### F02 P2：玩家服务分散保存Token与重复HTTP策略

上述四个默认传输各持有AccessToken并直接创建HTTP请求；Inventory已迁移GamePlatformOnline，而Online已有认证代次、single-flight刷新、取消/截止、重定向及预算机制。

整改要求：保持各领域业务Port及JSON转换，将认证/同源传输/刷新/重试预算委托Online；不能让Widget持有HTTP或Token。

核查入口：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Private/Transport/GamePlatformEntitlementGatewayHttpTransport.cpp`。原审查边界：设计缺口确认；不推断未经复现的具体账号泄漏。

### F03 P2：导航重复RequestId覆盖在飞查询

FindPathAsync使用调用者RequestId作为键，未拒绝重复，AsyncRequests.Add覆盖旧记录；旧查询/旧timeout仍存活。HandleAsyncTimeout只核对RequestId与WorldGeneration。

整改要求：登记前拒绝/幂等重复ID，或签发独立内部OperationId；回调还需核对引擎QueryId与请求代次。

核查入口：`Game/Plugins/GamePlatform/World/GamePlatformNavigation/Source/GamePlatformNavigationServer/Private/Subsystems/GamePlatformNavigationWorldSubsystem.cpp`。原审查边界：源码确认；需UE异步时序回归。

### F04 P1：任务冲突重放可能永久丢失权威事件

Reconcile先复制并Reset PendingEventPayloads，再逐个EnqueueDeferredEvent，忽略队列满的失败；加载快照应用失败时也没有保留重放原记录。另服务器落库前发布同Revision/Active的目标进度，客户端仅Revision更高或State变化才接纳。

整改要求：先保障重放接纳再撤销原所有权；满队列保留待处理并返回明确状态。区分持久化Revision与显示SnapshotSequence，保证同版本进度更新有明确接纳规则。

核查入口：`Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp`。原审查边界：源码确认；持久化适配器尚未真实联调。

### F05 P1：服务器准入Provider在飞回调缺卸载清理

HTTPAdmissionProvider的完成lambda捕获裸this并调用UntrackRequest。模块Shutdown直接Reset Provider；类没有关闭/析构取消全部请求并解绑的机制，CancelOperation也只取消不解绑。

整改要求：Provider明确Shutdown：停止接纳、撤销回调、取消自有请求、完成或抑制终态、确认安全后释放；请求状态可使用独立共享状态或弱Provider，避免裸this。

核查入口：`Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Source/GamePlatformServer/Private/Server/GamePlatformHttpAdmissionProvider.cpp`。原审查边界：所有权缺口确认；具体模块卸载崩溃需动态复现。

### F06 P2：Loading订阅重入可在Scope销毁后继续访问

订阅回调可以触发实例关闭/Deinitialize，后者Reset Scope；Tick在Callback后立即增加Scope计数，并在循环后继续读取Scope。回调前的非空检查不能保护回调后的访问。

整改要求：回调前保存作用域身份/代次，回调后重新检查同一Scope；关闭后立即结束本轮派发。

核查入口：`Game/Plugins/GamePlatform/Application/GamePlatformLoading/Source/GamePlatformLoading/Private/Subsystems/GamePlatformLoadingSubsystem.cpp`。原审查边界：源码确认；需回调触发退出的UE回归。

### F07 P2：装备组件销毁时没有撤销自己授予的GAS资源

SlotGrantHandles持有装备授予记录；RevokeAllRuntimeGrants只在换Avatar/应用快照调用，组件没有EndPlay/OnComponentDestroyed退出清理。

整改要求：把授予所有权绑定组件运行代次，在EndPlay/销毁时幂等撤销、失效异步回调并清空Port；回归ASC持续存活的组件移除。

核查入口：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Components/GamePlatformEquipmentServerComponent.h`。原审查边界：源码确认；真实GAS销毁/撤销运行待验证。

### F08 P2：中立表现目录未满足精确优先与确定性冲突合同

FCatalogScore先比较Scope再SemanticRank，ContentPack父语义能覆盖Project精确语义；父语义无显式允许/逐级回退。跨Fragment相同EntryId、同排序键、不同DefinitionId因EntryId相等不报Ambiguous，保留先注册候选。

整改要求：统一资格→精确语义→允许的逐级父回退→Scope/具体度/优先级；跨Fragment同键多候选失败，不用局部EntryId掩盖冲突。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/GamePlatformPresentationClientSubsystem.cpp`。原审查边界：源码与P13合同确认。

### F09 P1：VFX异步/延迟请求附着对象不是安全弱引用

AttachComponent仍为TObjectPtr，但完整Request保存于无反射引用收集的native Pending结构/DefinitionCache，Composite延迟lambda也按值保存请求；这些存储不提供GC追踪。异步完成后Niagara执行器以IsValid读取该地址并解引用，缺少附着目标所属World校验。当前InstanceRecord已缩减，不能沿用旧的“实例记录持有完整请求”证据。

整改要求：改TWeakObjectPtr；在每次异步完成/延迟触发时核对目标、Owner、World、代次和取消状态，拒绝失效附着。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXSpawnContext.h`。原审查边界：源码与UE ObjectPtr/IsValid实现确认；实际GC崩溃未运行。

### F10 P2：VFX复合Steps间接环校验未闭合

CompositeDefinition只拒绝直接自引用，Steps边未要求进入RequiredDefinitions；Data仅遍历RequiredDefinitions。A的Steps引用B、B的Steps引用A且RequiredDefinitions为空时，激活前验证仍看不到该环，运行时只靠MaxDepth截断。CompositeValidator“由Data阻断间接环”的说明不成立。

整改要求：确定Steps依赖唯一真源，纳入Data统一图验证或强制校验Steps与RequiredDefinitions一致；覆盖间接环、缺子定义及完整失败回滚。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Definitions/GamePlatformVFXCompositeDefinition.cpp`。原审查边界：源码及Data依赖遍历确认。

### F11 P2：SFX启动失败无回收，Pending/Active总预算可突破

有效AudioComponent即被登记Active，bAutoDestroy=false，仅靠AudioFinished回收；UE对bFailedToStart不广播该事件。另分别检查Pending<128、Active<256，完成时无总槽预留；255个Active仍可接128个Pending，随后活动数可到383。

整改要求：补失败/非活动回收路径，参数与委托配置后再播放；按Pending+Active预留总预算，回归Concurrency PreventNew、加载风暴与退出。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp`。原审查边界：源码与UE AudioComponent/AudioDevice确认；实际音频回归未运行。

### F12 P2：UI栈失活与最终移除混用，通知重复ID覆盖所有权

Manager在任何Deactivated事件移除屏幕加载/暂停/Travel账本，但CommonUI推B会暂时失活保留A，弹B又激活A；Manager没有恢复该账本。通知服务允许同RequestId新Widget/timer覆盖旧条目，旧timer又可关闭新通知。

整改要求：区分实例存在与显示激活两种生命周期；最终移除释放资源，激活切换更新暂停。通知在添加Widget前执行明确重复ID策略并清全部旧资源。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Manager/GamePlatformUIManagerSubsystem.cpp`。原审查边界：源码与UE CommonUI容器确认。

### F13 P2：输入重置作用范围与保存结果合同不一致

ResetMappings(None)重置整个KeyProfile，接口承诺只重置本配置已登记行；SaveInputPreferences调用原生void保存/Flush后直接bPreferencesSaved=true并返回成功，没有可观察磁盘失败结果。

整改要求：按当前Profile登记行逐行重置；保存结果区分提交/实际落盘/失败，采用可验证文件写入结果或修订公开合同并提供清晰状态。

核查入口：`Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Public/Interfaces/IGamePlatformInputService.h`。原审查边界：源码/接口确认；真实磁盘失败未测。

### F14 P2：Settings进程共享Provider持有可变用户键

模块唯一PersistenceProvider保存CurrentUserContextKey，GI Runtime仅切换账号时调用SetUserContext，保存请求不携带不可变用户键。

整改要求：Provider保持无账号状态，每次Load/Save携带UserContext/Scope/代次；或每GI独立Provider实例。保留UGameUserSettings的设备层进程共享语义。

核查入口：`Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsClient/Private/Persistence/GamePlatformSettingsClientPersistenceProvider.cpp`。原审查边界：作用域设计缺口确认；影响限定于可持久化多GI环境。

### F15 P2：终态历史缺有界策略：Data持续增长，PCG累计128次停止

Data ReleasedLeases永久保存完整已释放记录，无清理/容量策略，GameInstance长存时按累计租约数增长。PCG Requests在Cleaned后不移除，接纳要求Requests.Num()<128。

整改要求：分离活动所有权与有界历史诊断；明确幂等/过期墓碑的保留合同，PCG完成后释放活动槽，保留有限终态快照。

核查入口：`Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData/Private/Subsystems/GamePlatformDataSubsystem.cpp`。原审查边界：源码确认；不声称实测内存数值。

### F16 P2：统一Data租约仍未覆盖主要存量加载路径

Character HeroLoader、AI Definition/Brain及UI页面加载仍直接走UAssetManager/FGamePlatformAssetLoader/StreamableHandle，未统一实例/世界Data lease。Character当前真实消费者已有自身代次取消和版本比对，不能称其完全没有生命周期保护。

整改要求：逐类迁移并保留已发布主资产身份/序列化兼容；从真实消费者切换，再移除旧路径，不机械改全部类型或制造第二资产管理器。

核查入口：`Game/Plugins/GamePlatform/Gameplay/GamePlatformCharacter/Source/GamePlatformCharacter/Private/Loading/GamePlatformHeroDefinitionLoader.cpp`。原审查边界：调用路径确认；属于迁移未收口，不推断全部现存路径泄漏。

### F17 P2：短表现终态没有去重/取消墓碑

VFX/SFX只对活动RequestId去重，效果自然结束即删除映射；迟到Confirmed会重新播放。同样Cancel先于迟到请求时没有终态标记。

整改要求：维护作用域/代次隔离且有容量/期限的终态表，明确Confirmed/Correction与取消行为；复用Feedback已存在的有界发生ID思路。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp`。原审查边界：源码时序确认；需网络延迟/预测运行补证。

### F18 P2：插件描述未声明真实模块依赖

VFXClient/Editor Build.cs依赖GamePlatformCore，uplugin未声明；Debug Build.cs依赖GamePlatformCharacter，uplugin未声明。整体项目闭包还发现DBAWorlds→Core/Data及DBAArenaServer→Character漏声明。

整改要求：逐项补真实插件依赖及目标条件，保持模块Public/Private依赖准确；不靠关闭门禁或仅启用全部插件解决。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/GamePlatformVFX.uplugin`。原审查边界：既有门禁及117次独立目标装配确认；18条整体诊断含目标重复。

### F19 P2：正式基线仍有五个插件只维护模块壳

Localization、Camera、Animation、Lobby、Village共五插件十模块，仅描述/Build.cs/默认注册，没有对应可调用运行能力。前两类规格并不等于实现；Lobby/Village没有证明独立机制职责。

整改要求：Camera/Animation/Localization按真实需要实施或明确预留而不计已交付能力；Lobby/Village单独做身份/消费者/迁移/回退评审后决定收敛，不自动删除或更名。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/README.md`。原审查边界：实际文件清单与模块入口确认。

### F20 P2：客户端切片与真实业务闭环被文档混写

Equipment README宣称0004迁移/Outbox/DBAServer持久化适配已实现；Entitlement宣称Go领域/0003迁移/仓储/奖励链已实现；Commerce描述Backend/gameplatform/commerce与假支付实现，当前对应后端/组合源码不匹配。Quest也称DBAServer HTTP/事件适配但Port没有实现。旧全插件清单还把已实装SFX/Session写为旧状态。

整改要求：以当前文件/真实调用重写能力矩阵，分别列源码/模块编译/自动化运行/后端联调/Cook/人工验收；历史材料标时间与被替代状态。

核查入口：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/README.md`。原审查边界：对应后端/组合源码存在性及当前客户端实现确认；未全量审计Backend。

### F21 P3：Surface显式刷新不重新解析更换的MPC配置

RefreshMaterialBinding在旧BoundCollection/Instance有效时直接返回；公开合同称用于配置更换/热重载后重新解析绑定。

整改要求：显式Refresh解析新的软引用并比较身份，验证后原子替换旧绑定；普通Apply状态仍可复用稳定绑定。

核查入口：`Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Subsystems/GamePlatformSurfaceWorldSubsystem.cpp`。原审查边界：源码/接口确认；真实MPC运行未执行。

### F22 P2：自定义能力激活资格Gate仅声明未接通

IGamePlatformAbilityActivationGate说明无提供者应拒绝，但整个插件调用图没有实现/注册/查询；ASC输入仍直接TryActivateAbility。当前README已明确具体Grant由项目扩展，不能把完整Grant未实现当作虚假完成。

整改要求：明确Gate组合根与默认拒绝策略，并让真实激活入口覆盖；区分GAS原生资格与项目扩展资格，补未注入/过期Avatar/未Active用例。

核查入口：`Game/Plugins/GamePlatform/Gameplay/GamePlatformAbilitySystem/Source/GamePlatformAbilitySystem/Public/Interfaces/IGamePlatformAbilityActivationGate.h`。原审查边界：缺失接线确认；升级安全严重度需真实授权能力补证。

### F23 P2：交互焦点只在BeginPlay时判断本地拥有者

Interactor BeginPlay只有已LocallyControlled才建立FocusTimer，后续Possess/OnRep_Controller没有启动入口。

整改要求：由拥有者/控制器变化事件启停集中焦点采样；保留服务器权威验证，避免用业务Tick补偿。

核查入口：`Game/Plugins/GamePlatform/World/GamePlatformInteraction/Source/GamePlatformInteraction/Private/Components/GamePlatformInteractorComponent.cpp`。原审查边界：启动路径缺口确认；具体联机占有时序需动态复现。

### F24 P2：服务器准入响应上限只在完整接收后检查

Admission HTTP完成回调读取Response.GetContent后判断32KiB，没有像控制Provider那样使用流式受限接收。

整改要求：复用受限流接收策略及错误结果；验证超限响应在达到预算时停止继续接收。

核查入口：`Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Source/GamePlatformServer/Private/Server/GamePlatformHttpAdmissionProvider.cpp`。原审查边界：源码确认；未做网络预算测量。

### F25 P2：遥测专项门禁以全文件字符串搜索误报

TestTelemetryArchitecture搜索整个Sink中的Batch Move捕获，只要文件同时存在BeginSubmitBatch即判错误；实际BeginSubmitBatch已用独立RetryBatch，匹配来自另一个ScheduleRetry定时器lambda。

整改要求：限定检查到具体调用/函数，补安全/危险两个夹具；对重要时序以生产路径故障注入验证，避免关键词存在代替行为正确。

核查入口：`Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Tests/Scripts/TestTelemetryArchitecture.ps1`。原审查边界：脚本实跑失败及真实生产路径人工核对确认。

### F26 P2：AI服务器世界服务缺编辑器世界过滤

AI TargetRegistry世界服务只排NM_Client，未限制Game/PIE与Commandlet/Editor语义，注册路径可Spawn/Possess控制器；ServerOnly模块也允许Editor装配，UE默认WorldSubsystem支持Editor世界。

整改要求：ShouldCreate/DoesSupportWorldType及真实运行入口同时限制世界类型与authority，保证打开/编辑地图不改变运行Actor。

核查入口：`Game/Plugins/GamePlatform/Gameplay/GamePlatformAI/Source/GamePlatformAIServer/Private/Perception/GamePlatformAITargetRegistrySubsystem.cpp`。原审查边界：锁定UE宿主语义与源码路径确认；需编辑器非PIE回归。

### F27 P2：Gameplay候选出生Transform校验与实际生成不一致

公开SpawnCandidate合同说Transform为实际生成变换，GameMode区域校验使用Candidate.Transform，最终却RestartPlayerAtPlayerStart(Candidate.Source)，UE按Source的ActorLocation生成。

整改要求：在受保护出生操作内按Candidate.Transform生成，或明确合同要求与Source变换相等并拒绝偏移；补自定义策略候选回归。

核查入口：`Game/Plugins/GamePlatform/Gameplay/GamePlatformGameplay/Source/GamePlatformGameplay/Public/Types/GamePlatformSpawnRequest.h`。原审查边界：扩展入口缺口确认；默认路径目前一致。

### F28 P2：本地Save文件上限在完整读取后生效

SaveStorage将文件完整LoadFileToArray，8MiB限制直到Policy Decode才执行；主档和备份都经过该读取路径。

整改要求：读取前检查实际文件大小并限长读取，文件变化后也不能读超过上限；用超大主档/备份验证存储层失败。

核查入口：`Game/Plugins/GamePlatform/Application/GamePlatformSave/Source/GamePlatformSaveClient/Private/Storage/GamePlatformSaveStorage.cpp`。原审查边界：源码确认；磁盘/内存测量未执行。

### F29 P2：Progression/LiveOps事件合同未完整覆盖派生视图变化

Progression只有XP/升级事件：首次无旧轨道被跳过，删除/清空未完整通知。LiveOps跨开始/结束边界刷新后，仅服务器Revision提高才发目录/玩家状态变化，时间派生有效性可变化而Revision不变。

整改要求：增加状态/快照/派生视图代次事件，覆盖首次、错误、清空、轨道增删、活动时间边界与签到刷新；避免让UI轮询补缺口。

核查入口：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Services/GamePlatformProgressionClientSubsystem.cpp`。原审查边界：事件路径缺口确认；LiveOps派生事件影响需真实边界场景验证。

### F30 P3：公开API中文说明与人工审核证据仍有存量缺口

部分Build.cs只有英文规则；Equipment服务器公开初始化/请求API、Progression传输/账号契约、Entitlement领域字段等缺逐项中文前提/线程/所有权/失败和取消说明。

整改要求：以本次涉及文件建立存量清单，修复时同步补齐相关代码路径与中文接口/配置/测试说明；自动扫描只辅助人工内容审核。

核查入口：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Components/GamePlatformEquipmentServerComponent.h`。原审查边界：代表性缺口确认；没有量化全量注释合规率。
