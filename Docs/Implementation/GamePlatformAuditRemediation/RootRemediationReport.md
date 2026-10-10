# 基础服务、诊断与装配整改记录

适用2026-10-09隔离分支，基线8a12bbe，历史修复三方整合提交0f6afcb。本文是人工维护的中文说明，记录真实源码行为、所有权及兼容影响。实际构建结果以执行台账为准，新增UE测试未执行时不能计为通过。

| 审查ID | 源码处置 | 验证与剩余条件 |
|---|---|---|
| R-01 | Data已签发租约使用私有密钥与完整身份摘要验证，释放后不保存每份Lease历史；合法重复释放幂等，篡改、跨实例拒绝 | 原生策略Debug/Release通过；真实AssetManager多实例、GC、资源占用仍须UE运行 |
| R-02 | 有请求才启动0.25秒所有者维护，无请求停止；WorldCleanup事件即时撤销，查询失效上下文立即返回Released | 周期属于清理时限，未测CPU/GPU收益；没有以单测证明无性能问题 |
| R-03 | 准入Provider取消/关停不保留裸this完成引用；子系统先取走账本再通知，永久关闭拒绝新请求，旧同ID操作先核代次再移除 | Reset/Stop两种所有权分离；正常排空保留活跃绑定，通知内世界退出可升级完整Reset；UE回归待运行 |
| R-04 | 准入HTTP接收流式计数并在预算前拒绝，响应与请求终态受门闩保护 | 使用锁定HTTP重定向补丁；真实握手、后端拒绝、压力与超时仍待联调 |
| R-05 | Telemetry HTTP完成委托不强持有自己的Request；独立流式响应限额不保存响应正文；启动失败与HTTP完成共用一次终态 | 新响应预算纯算法边界/负值/溢出回归Debug/Release通过；实际HTTP取消、网络异常仍待运行 |
| R-06 | 缓冲、Schema Registry实现移入Private；稳定Schema值类型保留Public/Types，旧路径只含不透明声明/值类型包含 | 仓库无外部构造消费者；外部直接构造内部类的源兼容不承诺，需迁移公开服务并重编 |
| R-07 | 聚合定义审计与实际Editor Validator均按DefinitionBase、LogicalId、PrimaryAssetId、DataVersion验证；派生类集合取一次，图用显式栈 | 内存夹具包含合法/重复/版本与两个验证入口；尚未执行UE；大型资产库审计成本、真实派生资产/硬引用须补证；无性能场景执行器时保持Unsupported |
| R-08 | DBAServer完整承载World退出后永久标记当前Profile退休，立即Reset准入，停止心跳并排队Drain，迟到Ready不重新开放 | 同Boot承载新World需重新启动和Profile验证；区域流送不走完整退出；实际三角色启动/退出待运行 |
| G-01 | 补齐DBAGameplay Combat、DBAArena Ability/Combat/Gameplay、MOBA Arena Server、Surface Data及DeveloperTools真实直接插件依赖 | 声明闭包和构建规则同时核验；真实UBT链接结果另列 |
| G-02 | 主工程Common/十二Hero纯表现入口及依赖排除Server，Stage排除对应目录；保留DBAGameplay权威定义和Village共享世界 | 实际Server声明根闭包回归红转绿；最终干净Cook/Stage、必要权威动画和硬引用仍须产物审计 |
| G-03 | 总体规划/插件规范同步46代码＋16内容、82模块、DBAClient五模块、Boar及已存在前端入口 | 已登记原型不等于正式资源交付；保留历史执行数量与原文上下文 |
| G-04 | Data/Server/Telemetry/Debug/DeveloperTools新增或触及接口补中文参数、单位、端侧、线程、取消/异常、所有权与兼容说明 | 全工程未触及存量未完成逐行人工中文审核，不宣称整体合规 |

## 独立复核追加问题

控制面失败通知可以同步Drain或Deinitialize。当前先建立原操作重试所有权，再广播不可变快照；Drain只排队，注册/Ready必须正确完成或失败，不能由通知重入跳过。永久关闭及操作代次阻止返回后重新建Ticker、发送网络操作或发布Ready。测试用明确内存状态夹具验证两种通知重入，不代表真实Provider重试联调完成。

正常StopAccepting与完整Reset使用独立栅栏。Stop取消Pending的完成通知可以触发World退出或Deinitialize；更强的Reset仍可取走Verified并撤销Target。重复Stop不重新打开握手，完整Reset期间不允许重配；Deinitialize后永久拒绝配置。测试通过真实公开Stop/Reset/Deinitialize调用覆盖这两个回调路径，尚须引擎执行。

Data的ExpectedClass是加载对象IsA约束，可使用抽象领域根类；不会实例化约束类。仍拒绝空、Deprecated、NewerVersionExists，实际Definition对象继续核身份、版本、依赖和内容。VFX抽象根类的原合同冲突已在数据服务修正，表现组负责跨模块回归；缺真实定义不能被当成加载成功。

## 兼容与回退

VerifiedAdmission新增MatchId投影，来自现有已验签后端响应；普通世界允许空，不能推算为AssignmentId或GameSessionId，未修改Shared真源/生成物。C++消费者需要重编。新增StopAccepting/Reset不改变安全握手票据格式。

DBAClient五模块明确仅Game/Client/Editor目标包含，含原Runtime命名的项目表现定义模块；后缀不作为隔离机制。实际仓库只有Client/Server/Editor三个Target，本轮没有删除普通Game允许范围，也没有虚构不存在的Game目标构建。服务器若曾主动启用公共客户端Runtime，必须迁移到低层中立合同；服务器默认组合没有该引用。

Telemetry保留原类型符号和旧公开路径，不再暴露私有策略类定义；GetSchemaRegistry是弃用的不透明兼容入口。必要Schema结构仍可供扩展注册。回退时供给/消费者与公开包含路径须一起恢复，不能只搬一个头文件。

审查整改的源码/配置修改在隔离分支内，不回写原main或其他活跃工作树；未改变引擎源码、后台或生产服务。用户后续授权的60枚生肖图标任务在原main工作区独立执行，其图源、Monolith资产与编辑器构建产物不属于本审查分支的源码验证。服务器资源规则回退还需重新构建和烘焙验证；配置差异不能证明现有包已经改变。

## 本轮真实编译修正与输出器边界

锁定UE5.8 Server模块构建暴露的Timer接口、TObjectPtr推导、测试枚举/SharedPtr、lambda成员引用、头文件完整类型与Telemetry不透明私有策略构造/析构问题均按真实编译器诊断修正，没有删除测试或关闭模块。45个服务器端模块增量重试RunId9d638e88-541f-4e13-84df-11f2bdc326d4退出0；这是模块构建，不是完整服务器程序、UE测试运行或烘焙验收。

客户端首次构建680d1f5e-f1e9-4c26-84fe-d5300709968f退出6，四条诊断来自Presentation公开注册接口按值接收TSubclassOf却缺少模板完整定义。已在该公开头显式包含Templates/SubclassOf.h，并说明独立模块编译前提；未改变注册资格或默认值。后续独立RunId重试以UEBuildResults.json为准。

Telemetry追加Sink生命周期整改采用永久关闭、实例/Sink/Ticker/完成代次及一次终态；上下文Begin/End/Travel/UpdateWorldContext使用独立ContextOperationGeneration，换输出器不会吞掉账号/世界边界，真正的后继上下文则取得发布权。八个真实Subsystem/可重入Sink用例已被服务器模块编译，尚未运行UE Automation；独立窄复核结论仅限已确认链的源码。

Foundation构建入口增加UBT的-WaitMutex：Foundation自己的锁只覆盖该脚本，UBT锁还覆盖编辑器或同引擎其他项目的合法调用。等待仍计入TimeoutSeconds；超时只终止本次受控进程，不能中止其他项目。脚本语法检查和客户端重试会验证此路径，不将等待当作编译通过。
