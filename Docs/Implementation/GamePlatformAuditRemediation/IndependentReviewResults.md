# 独立源码复核结果（2026-10-09）

本轮三组复核以当前隔离分支实际差异、公开合同、主要失败/取消/清理路径和新增回归源码为证据。复核者保持只读，没有运行UBT、UE Automation、Cook或网络。这里的关闭仅指确认触发链已修，不能代替引擎行为验收，也不扩展为全工程无缺陷声明。

| 领域 | 首轮结果 | 再次冻结后的定点复核 |
|---|---|---|
| 玩法／世界／竞技／服务器 | 7项Important：Quest按任务重放、广播重入、完整快照；Server控制重试与排空；导航旧句柄；交互终态身份 | 原触发链均修；追加发现的Quest未声明变量、停止通知升级Reset、死亡通知结束比赛、旧Adapter资格所有权已修。最后两链复核未发现未解决Important，7个冻结源码哈希匹配 |
| 应用／玩家服务／Editor诊断 | 9项Important：Settings读取/Save/关闭；Inventory恢复；Equipment候选授予；整数和集合；LiveOps请求；真实Editor Validator | 已修主要链；无Provider代次、准备失败事件和监听器接管又经定点修正。最后Settings/Inventory两链复核未发现未解决Important，真实订阅者回归已加入 |
| 表现／MOBA事实／项目内容激活 | 8项Important：Data抽象约束；VFX自然清理与纠正请求；UI替根/替VM；Composite必需子拒绝；MOBA世界代次和取消接线 | 8链均修，最后只读复核未发现剩余Important；78个冻结交付文件哈希前后匹配 |

没有确认Critical。首轮三组共24项Important；后续定点发现继续交原责任组修正，不把初次源码冻结称为最终集成通过。复核没有重扫已确立且未变化的全部代码；未读取的存量源码和未运行的资产/网络不能据此宣布通过。

## 最后复核确认的所有权

- Quest以玩家运行身份和任务实例保存重放归属，广播后重新查找，损坏快照不能吞掉待提交事件。
- 正常排空停止新准入，完整世界退出仍可在取消通知内撤销既有授权；旧请求不能移除同ID的新请求。
- Arena死亡统计广播和外部资格命令返回后核同世界、比赛、装配、玩家、Pawn与代次；复活Timer先核RequestId再清账，到期再核比赛。旧Adapter不能操作新Pawn资格。
- Settings准备失败统一发布一次非Loading终态；持久层缺省采用明确默认层合同，不能重复推进本次请求代次。Inventory监听者接管查询/对账后旧栈停止。
- Equipment候选授予保活端口/定义，外部GAS调用后复查寿命；过期候选撤销自有句柄。金额和整数字段验证原始token及范围，错类型集合失败且不覆盖旧视图。
- VFX终态按账本所有权清理，允许Niagara先失活再Finished；纠正播放通知关闭后不申请新租约。Composite未受理的必需步骤向根传播失败并清理兄弟。
- UI替根和替VM在外部通知后重验关闭、布局/选择/激活代次；MOBA首次Travel请求由同次Submit补齐平台世界代次，延后接线可取消且拒绝旧回调。

## 回归证据限制

回归源文件放正式模块Private/Tests，由真实UE构建纳入；原生条件入口另由CMake编译。Transient夹具、内存状态和测试Transport明确只提供测试前提，不是生产提供者。

Data/VFX跨模块用例消费真实公开数据服务，覆盖抽象约束受理、缺定义失败、普通软资源租约与完成一次；自然结束用例按引擎事件顺序驱动组件。没有真实Niagara System就不能把该用例称为完整特效播放验收。

最后Arena三个用例覆盖正常/旧请求、统计监听者结束比赛、排队后结束；旧装配资格和Timer隔离也有断言。实际到期Spawn/Possess、独立真实WorldExit场景、联机和五模式资源仍需要运行补证。

复核不将原生82次执行用于证明UObject、GAS、HTTP、CommonUI、AssetRegistry或网络结果。实际构建和执行结果继续见ExecutionProgress、ValidationSummary与整合交付说明。

## Telemetry额外生命周期与上下文定点复核

自定义Sink同步Start关闭、Shutdown重入和GetHealth换代三链已通过closing/实例及Sink代次闭合。首次修复引入的上下文边界提前返回也已修复：账号和世界边界采用独立ContextOperationGeneration，单纯换输出器不取消必要清理，真实后继命令接管才阻止旧栈。最后只读复核确认头、实现和八个生命周期用例源码摘要与冻结报告匹配，未发现该窄范围未解决Critical/Important。

八个UE用例已纳入本轮真实服务器模块编译；UE Automation尚未执行，无网络测试、跨GI、多PIE或关闭预算实测结论。详细文件和兼容影响见TelemetryLifecycleRemediation.json/md。
- 编译追加窄复核：独立复核Presentation公开TSubclassOf完整头、Foundation-WaitMutex与默认false的UsePrecompiled。对照锁定UE5.8真实GlobalOptions/UBT源码与FoundationTools进程所有权，未发现Critical/Important；语法0错误、两文件diff检查0。检查确认等待计入原超时、不夺取其他进程，预编译mode实际记录且不自动跳过项目源码模块。未重扫原关闭链、未重跑目标构建，不增加运行通过数。
- VFX编译追加窄复核：独立检查InstanceRegistry直接包含Engine/World.h以及私有PresentationProvider构造外置；原Engine依赖、类布局、构造签名和World弱所有权保持，同模块消费不需要新导出。三文件diff检查0，未发现Critical/Important；不重扫运行链、不增加Client构建或实播通过声明。

- Telemetry编辑器首包含定点复核：两个稳定cpp先包含自身同名Public兼容头，再包含Private完整定义；前者只有同一类型前置声明/既有值类型，未生成第二份定义。类型身份、导出宏和依赖不变，中文职责、游戏线程和GI所有权说明已审核，两文件diff检查0，未发现Critical/Important。此结论只关闭首包含源码缺陷，真实Editor重试仍待记录。
- Editor模块真实链接依赖窄复核：PresentationClient公开结果值补Public Core；SurfaceClient内部结果/身份调用补Private Core及排除Server的插件声明；ArenaServer公开Assignment补Public MobaCore、内部ASC桥补Private GameplayAbilities及Server/Editor插件声明。直接导入与公开头匹配、依赖保持向下且无循环/客户端服务器互依赖；五文件空白和两个JSON解析0，未发现Critical/Important。修前真实LNK诊断保留，消失与否仍以增量构建结果为准。

- 最后MobaData／内置身份／装备夹具只读复核：ArenaServer的Private MobaData对应死亡桥Find/Duel1v1实现调用，未公开该模块类型且依赖向下无环；Architecture仅精确增加真实UE5.8 GameplayAbilities身份，未知身份仍被拒绝。Equipment局部const IVS与原五项参数一致，一次传入CreateWorld，保留空值失败与销毁；未发现该三文件范围Critical/Important。实际空白检查0、脚本语法0错误、三文件SHA回读0差异，未计作Pester或UE运行。证据为Saved/Validation/PluginRemediation-2026-10-09/FinalMobaDataEquipmentReview。
- 四领域Automation注册名独立复核：以真实引擎宏类名注册键为依据，四个唯一宏与RunTest限定名一致，PrettyName/Flags保持；规范化反消本轮允许差异后全文SHA与修前捕获一致，未发现该窄范围Critical/Important。修后完整136项已实际发现，行为与四项专项仍须真实运行，不把注册成功当测试通过。
- VFX测试标签只读复核：ClientOnly中的Native定义触发非Fatal ensure，不能推断标签一定没入字典；局部读取当前及HEAD既有Presentation.Test，缺失以AddError/false停止，DefinitionId保持空，真实Registry仍走精确语义和Scope/类型资格断言。无新生产标签或全局标签构造，不改宿主类型；未发现阻断问题。根将中文说明限定为避免本测试新增客户端独占标签，整端字典和Cook不作认证；修后编译/运行另记。

- LocalPlayer/Viewport六文件真实引擎合同独立复核：11处实例均使用GEngine，保留显式失败前提及Transient范围。首次确认Moba Controller只有单向PlayerController赋值，世界查询无法识别其LocalPlayer；根补真实SetPlayer后复读闭合。三个Moba世界、UI重入世界和玩家注册都有作用域清理，未发现该范围剩余Important。报告保留初次问题及最终六文件哈希；这不是11项运行通过。

- 三Gameplay测试夹具独立复核：实际读取UE5.8 Controller/GameState/Actor与Timer/Automation销毁链，三SHA在始末匹配，Critical/Important均0。共享owner先摘Mode接口、清自有Timer/监听再释端口和World；只覆盖自有资源释放/unroot，测试World未完整BeginPlay时EndPlay返回false，未把该结果写成全部Actor EndPlay/GC验收。真实五失败修后回归仍须执行。
