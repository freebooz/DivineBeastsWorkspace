# 插件设计、性能与项目规范审查报告

审查日期：2026-10-09。工作空间：E:/poject/feebooz/DivineBeastsWorkspace。分支：main。源码基线：8a12bbe1d1d02db835b92207646138fabe782b50。

## 结论与证据边界

当前插件架构已经具有可复用机制、三层方向、角色策略和多处资源／请求所有权设计，但不能认定全部设计完善、生产链完整或性能已验收。62个实际插件全部有独立审查条目；评估依据为实际描述、构建规则、Public合同、主要实现与失败/取消/退出路径、调用方和现有测试。核心路径详读，其他文件以声明/清单/搜索补充，不声称全源码逐行认证。

性能结论分为可确认的增长/容量/时序风险和需要测量的扫描/同步加载热点。没有CPU/GPU/内存实测、设备基线或Unreal Insights记录，不能写“无性能问题”或“性能最佳”。“未发现阻断”仅指本轮已读范围。D0=设计/注册壳；D1=源码机制存在而集成/资源不足；D2=有消费者及真实原型资产；均不是生产通过级别，不计算虚假百分比。

5个插件仍只有注册壳：Localization、Camera、Animation、Lobby、Village。Camera方案和部分稳定身份保留有现行规划依据；应明确实现与保留决策，不能计为能力已交付，也不能擅自删除既定身份。Settings、Equipment和4个玩家服务另有机制但缺生产Provider/账号装配，不能与完全空壳混为一类。

必须先解决的正确性问题包括：多竞技World覆盖唯一适配器并留下裸指针；GAS激活资格和真实角色装配缺失；竞技票据准入无调用方、重连不恢复Pawn；Quest对账丢重放材料。正式网络World入口Unsupported与5模式NotConfigured是明确拒绝后的交付缺口，避免将正确拒绝误写成“功能通过”。

跨领域关键问题还包括：PCG第129次请求永久容量失败、Data释放历史无限增长、Save读前容量未限制、SFX总实例预留不完整；目录精确语义/歧义规则不符合P13、Composite依赖环绕过Data；项目内容激活先发布后只广播预载；UI/Loading/玩家服务重入与账号事件；输入保存/映射所有权；服务器英雄包声明隔离不足；中文公开合同和正式文档未同步。

上一轮修复提交554ee8ab11328e765d2f7c9bf1bc788a712905e5不在当前main祖先历史中；本报告不沿用其修复或通过数。读取根AGENTS、总体规划、插件规范及三层实施说明；未发现祖先/任务范围局部规则。独立《整合与交付说明》未检索到，现有三层实施文件只作为历史整合证据。

## 本轮执行的检查

| 检查 | 本轮实际结果 | 含义和限制 |
| --- | --- | --- |
| 当前插件盘点 | 62描述：46代码／机制＋16内容；82源码模块 | 39平台＋Arena稳定身份共40GP；独立MobaPresentation＋5项目代码；纯内容0模块 |
| 架构Pester | 72项：69通过、3失败、0跳过 | 失败对应插件依赖声明；不是72项UE测试 |
| 结构及六种声明装配 | 退出1；18条重复诊断、4条独立缺失依赖 | DBAWorlds→Core/Data；VFX→Core；DBAArena→Character |
| 公开C++继承边界 | 退出0；437头、905类型、149继承边 | 不证明Blueprint/DataAsset硬引用和父类 |
| 自有头文件存在性 | 退出0；465处引用、0缺失 | 不验证链接、API兼容和实际UBT可见性 |
| 英文命名 | 退出0；48978项、62描述、0违规 | ASCII扫描；不验证中文说明质量或完整职责命名 |
| 原生C++检查 | 13测试目录，Debug/Release共26次构建检查；25个CTest用例各两配置，共50次执行，全部退出0 | 真实生产算法的现有原生回归；不覆盖UObject/World/网络/资产 |
| Save／Settings／DBAFlow／DBAInput专项门禁 | 4脚本退出0 | Settings明确警告无生产Provider；只是静态结构 |
| 实际Server声明根闭包 | 包含DBAClient、DivineBeastsPresentationRuntime | 不等于已Cook/链接纯表现制品；详见JSON |
| Git差异 | 审查前及生成报告前工作树干净 | 最终收尾状态另核；仅Saved证据文件 |

报告及检查证据均在Saved/Reviews/PluginAudit-2026-10-09，未修改源码、配置、资产和正式文档，未创建提交/推送。当前Monolith返回编辑器未运行，本轮没有资产编译/回读。候选UE5.8目录缺Build.version和Engine/Source，本轮未确认可执行锁定构建环境，不能据此断言整台机器没有引擎。

本轮未执行：UE Editor/Client/Server/Game目标构建、UE Automation、干净Cook/Stage、服务器制品审计、AssetRegistry/DataValidation资产父类/引用、真实网络与3角色/5模式验收、Monolith Widget树/导航/焦点/编译保存重载、设备CPU/GPU/音频/内存测量。内容16包磁盘79个资产文件（76uasset/3umap）只代表文件清单；项目DBAGameplay另有12个Hero定义，不能混成完整英雄资源或运行证据。

## 平台层：39插件

| 插件（链接至实际描述） | 设计完善度／当前缺口 | 性能问题与待测成本 | 项目规范核对 |
| --- | --- | --- | --- |
| [GamePlatformAbilitySystem](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformAbilitySystem/GamePlatformAbilitySystem.uplugin)（1模块） | GAS基类、ActorInfo/输入代次与AbilitySet字段校验；Gate/授权事务未落入ASC生产路径 | InputTag每次按压/释放扫描全部AbilitySpec；WhileHeld每处理尝试激活，缓存查询为线性；未实测。 | 输入精确标签冲突失败、换Avatar先清输入、每ASC独立随机作用域、Mixed复制策略。 涉及 GW-01、GW-02。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformAI](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformAI/GamePlatformAI.uplugin)（2模块） | 共享事实与服务器BT/感知/目标/导航/GAS攻击源码；正式BT/NavMesh运行未验证 | 一次初始化Actor扫描；定时决策扫描候选，Prune超限循环每次再全扫，超限突发为平方量级；入候选到决策间没有即时数量裁剪。 | 目标Provider+EntityId/Generation同世界校验、无每帧全世界扫描、攻击退避、取消加载/解绑路径存在。 涉及 GW-12。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformAnimation](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformAnimation/GamePlatformAnimation.uplugin)（2模块） | 2模块空壳；README明确一期关键能力未完成 | 没有实现可评估动画成本，也没有真实动作/GAS/RootMotion时序验收。 | 未用固定成功接口伪装完成；已有实施规格说明。 涉及 GW-13、GW-12。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformApplicationFlow](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/GamePlatformApplicationFlow.uplugin)（1模块） | 机制实现较完整；尚无本轮动态验收 GI唯一执行器、单槽完成邮箱、作用域/运行/节点代次、有限重试/超时、关闭和定义租约已落实；按需Ticker与核心分发/广播重入门禁可见。 | 单槽邮箱有界；图校验std::map/set为O((V+E)log V)，主要为启动成本。APP-13指出重复Completion的TaskGraph唤醒尚未合并；无实测结论。 | 方向中立、Public稳定合同和中文说明相对完整；无游戏/竞技/项目资源依赖。 |
| [GamePlatformCamera](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/GamePlatformCamera.uplugin)（1模块） | D0。0.1.0；唯一 ClientOnly 模块只依赖 Core、仅3行注册。方案A已批准但实现未开始。没有服务、模式栈、租约、测试、实际消费者；不得称相机功能已完成。 | 无运行机制，无法评价相机算法性能。当前预览相机在 Presentation/项目预览实现中，不能记为 Camera 插件集成。 | 按源码职责/端侧/中文说明核对；D0。0.1.0；唯一 ClientOnly 模块只依赖 Core、仅3行注册。方案A已批准但实现未开始。没有服务、模式栈、租约、测试、实际消费者；不得称相机功能已完成。 动态验收仍待证。 |
| [GamePlatformCharacter](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformCharacter/GamePlatformCharacter.uplugin)（1模块） | 可复用Definition/创建Provider/初始化Executor/异步加载基础；不是完整角色战斗装配 | 冷定义异步加载，热对象同步完成；无角色业务Tick；需测量大批出生的定义预热与包络成本。 | 唯一初始化器Fail Closed、服务器Authority校验、不拥有Spawn/Possess；公开通用定义不绑定项目表现。 涉及 GW-02。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformCombat](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformCombat/GamePlatformCombat.uplugin)（1模块） | 伤害/治疗/控制/GAS属性/死亡/当前帧Trace实现；网络与正式玩法组合未验证 | 无Tick；事件去重有界但RemoveAt(0)移动历史数组；属性大量COND_None复制，需要按可见性与网路实测。 | Authority、同World、代次/数值上限验证；未伪实现Root/ServerRewind；可选表现不决定权威伤害。 涉及 GW-02。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformCommerceUI](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/GamePlatformCommerceUI.uplugin)（1模块） | 购买意图/订单展示状态机制已实现；支付与认证生产链未装配 每种请求single-flight、AccountGeneration、只有Backend fulfilled+confirmed才Succeeded、Receipt不直接判成功；无生产Configure/HTTP构造/真实支付Provider，不应称商城闭环。 | 8HTTP在途；Catalog与订单值复制、多JSON解析在GameThread、无响应体/数组容量门禁；金额转换double→int64未拒绝小数/2^53精度风险，不构成当前权威扣款错误。 | 依赖GamePlatformUI单向、支付权威边界正确；Public具体HTTP/AccessToken与大段公开字段说明不全，ClientOnly无TargetAllowList；真实Widget资源未交付。 |
| [GamePlatformCore](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Foundation/GamePlatformCore/GamePlatformCore.uplugin)（1模块） | 中立身份、错误、版本、结果值与私有解析算法已实现；本轮未发现阻断性设计缺陷。 | 输入长度/字段范围有界；值校验、规范化与哈希重复解析/分配，应在真实热路径测量，不能仅凭微测试认定瓶颈。 | 不依赖Engine/业务/世界，默认结果非成功，Public主要契约中文说明完整；13个现有CTest用例各在Debug/Release通过。 |
| [GamePlatformData](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Foundation/GamePlatformData/GamePlatformData.uplugin)（2模块） | 统一主资产定义、依赖DFS、跨实例分组并集及租约实现；终态历史无界。普通软资源已有平台薄封装，其作用域安全需核各调用方。 | ReleasedLeases随全生命周期释放总数增长；每帧扫描全部活跃请求，资产依赖最多128深度/4096节点。 | GameInstance/World/代次与弱调用者校验、取消一次终态、失败撤销自有需求；普通软资源通过Data薄入口返回原生流式句柄，由调用方取消/释放。 |
| [GamePlatformDebug](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformDebug/GamePlatformDebug.uplugin)（2模块） | 开发/测试诊断提供者、命令注册、目标弱引用与快照脱敏已实现；未发现当前分区阻断性功能缺陷。 | 按需采集，昂贵Provider显式授权；每次快照清理所有曾查询目标，规模成本需測量；无每帧全世界扫描证据。 | Shipping配置白名单排除，进程注册表持机制与弱目标，没有持有用户/比赛强对象。部分新增公开/私有函数中文说明仍须人工补齐。 |
| [GamePlatformDeveloperTools](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformDeveloperTools/GamePlatformDeveloperTools.uplugin)（1模块） | Editor验证/静态与资产规则/报告/性能执行器接口存在；性能场景无生产注册调用，资产扫描以名称判断定义有覆盖边界。 | GetAllAssets后筛选、GetAsset同步加载所有定义；属Editor全量审计成本，应测量大内容库内存/耗时，不能归为游戏帧率瓶颈。 | Editor专用，未知性能执行器明确失败，不伪造测量；C++继承门禁通过仍不能代替Blueprint资产检查。 |
| [GamePlatformEntitlement](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/GamePlatformEntitlement.uplugin)（2模块） | 权益快照/查询机制已实现；生产认证装配缺失 LocalPlayer账户代次、Reset取消、Revision不回退、有效ID/英雄/皮肤派生Set缓存；有效性以后台Snapshot.Status为准；无生产Configure或HTTPTransport构造调用。 | 查询Set平均O(1)，查询类HasAny/All逐数组扫描可能O(KN)，View值返回仍复制；Transport8在途但回应数组与总JSON无容量预检。 | 客户端与后台权威区分；具体HTTP实现在Public/Transport且构造接收AccessToken，应收敛为Private+Online安全通道；端侧allowlist/中文字段说明不全。 |
| [GamePlatformEquipment](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/GamePlatformEquipment.uplugin)（3模块） | 共享复制+GAS/外观机制部分实现；生产持久化装配缺失 OwnerOnly与Public快照、服务器校验/持久化端口、GAS授予、客户端Data软加载与视觉代次分离；无实际PersistencePort/AssetResolver实现或Initialize生产调用。 | Apply每次撤销全部授予后全量重建；Visual每次清全组件/加载后重建，O(S+A+E)并伴对象/加载抖动；无slot数上限或性能数据。 | 无客户端/服务器互依赖；端侧标签有但无TargetAllowList；服务器私有实现合同缺中文与取消/线程说明，只有Definition测试不能冒充生命周期验收。 |
| [GamePlatformGameplay](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformGameplay/GamePlatformGameplay.uplugin)（1模块） | 体验/准入/出生/玩家生命周期与复制资格源码已存在；当前真实资格接线缺失 | 0.05秒GameMode全玩家推进、等待出生重复PlayerStart全扫描；Experience另有0.05秒轮询。建议测量等待状态和并发玩家成本。 | 权威登记、作用域/代次、自动出生阻断、取消/排空路径与公开资格事件存在。 涉及 GW-02、GW-11、GW-14。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformInput](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/GamePlatformInput.uplugin)（1模块） | 机制实现较完整；持久化/所有权缺陷；真实Profile未装配 LocalPlayer作用域、Data定义租约、CompactSlot、阻断缓存、Owner回收4Hz、Touch与统一EnhancedInput事件链可见。 | 高频阻断O(1)，订阅/绑定/触点有界；显式Save同步SaveSettings/Flush可能卡菜单；ListMappings每次构造排序O(RlogR)。 | 新平台中立语义与项目语义分离较好，ClientOnly+明确targets；公开中文合同承诺与实现存在APP-01~03差异。 |
| [GamePlatformInteraction](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformInteraction/GamePlatformInteraction.uplugin)（1模块） | Focus/Instant/Hold/目标并发/幂等/限流/外部结果Reservation源码；Custom回调重入有悬垂引用风险 | 无Tick；Focus定时Trace、Hold定时复验；反复资格读取遍历Actor/Pawn/Controller/PlayerState组件，按实际组件量测量。 | 资格默认拒绝、组件级目标身份、独立Begin/Cancel限流、幂等缓存、共享会话不会被无关Revision误杀。 涉及 GW-09。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformInventory](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/GamePlatformInventory.uplugin)（1模块） | 六服务中生产认证接线最完整；仍无本轮运行验收 Online认证自动装配、账户/请求代次、单未决Mutation、原OperationId未知结果查询、RevisionConflict专用对账、64容器/1万物品/12快捷槽验证可见。 | 缓存排序O(NlogN)仅Snapshot变化，FindItem缓存查询；值数组Get...仍O(N)复制，View路径可避免；Transport8在途，上限不代表当前测量。 | Transport在Private、不持Token，业务请求走Online、客户端不持长期真源符合规范；公开参数/线程/失败中文尚不全，模块无TargetAllowList。 |
| [GamePlatformLiveOps](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/GamePlatformLiveOps.uplugin)（1模块） | 运营快照/领取对账/可信时间部分实现；生产认证接线缺失 LocalPlayer代次、全局Catalog与玩家状态分离、ClaimOperationId对账、服务器时间+单调时钟、前台恢复刷新可见；无生产Configure/HTTP构造。 | Initialize无条件1HzTicker，即未登录也持续唤醒；边界每秒扫描S+E+C；GetSignInViewModels每Campaign对PlayerState线性查，O(CP)，每次重建数组，无输入容量上限/测量。 | 后台领奖权威未被客户端替代；缺完整中文接口/参数/错误/时间单位，README与Docs不足；Public具体HTTP/Token/内部时间类、target允许列表不足。 |
| [GamePlatformLoading](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformLoading/GamePlatformLoading.uplugin)（1模块） | DAG/租约/屏障已实现；生命周期回调缺口 单操作GI、任务规格256/依赖64、订阅/工厂128上限、Data租约、真实世界事实、20Hz运行/2Hz保留资源监测已具备；SessionReady需真实上层装配。 | 上限存在；执行遍历中每Task再次线性找PolicyRecord（343-344），O(T²)每20Hz且GetSnapshot构造数组，需256任务测量，不可称已优化到O(T)。 | 不依赖客户端Session/UI/Flow；中文公开合同较清楚。Runtime描述缺TargetAllowList，应补目标审计，不能靠Runtime名称算四目标验收。 |
| [GamePlatformLobby](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/GameModes/GamePlatformLobby/GamePlatformLobby.uplugin)（3模块） | 3模块空壳；无活动源码消费者；现行规则明确条件保留 | 无实际运行机制可测；不应宣称已提供大厅服务器。 | README明确Lobby属于OpenWorld体验、不恢复独立服务器角色。 涉及 GW-13、GW-12。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformLocalization](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformLocalization/GamePlatformLocalization.uplugin)（1模块） | 二期预留空模块 未实现文化切换、文本资源域、客户端加载、事件、配置或运行接口；README已承认预留模块壳。 | 没有本地化逻辑，不能评价业务性能；只有空模块装载/维护负担。 | 与AGENTS禁止空插件/空模块冲突；活动FriendlyName为英文但源码/Build.cs无职责中文合同；ClientOnly无TargetAllowList。建议按批准规划明确退休/禁用或在真实需求后实现，不能擅自删除。 |
| [GamePlatformNavigation](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformNavigation/GamePlatformNavigation.uplugin)（2模块） | 原生导航桥接、同步/异步、取消/超时/Invoker/Profile实现；重复RequestId生命周期缺陷 | 同步FindPath/TestPath调用在调用线程执行；异步总量有上限；调用方应限制高频同步查询并实测NavData规模。 | MaxPathDistance/MaxProjectionExtent、PartialPath、世界代次、取消/超时与原生导航能力复用。 涉及 GW-07、GW-12。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformOnline](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/OnlineServices/GamePlatformOnline/GamePlatformOnline.uplugin)（2模块） | 公共门面与客户端HTTP、刷新单飞、有界排队/重试/流式响应限制已实现；锁定HTTP补丁与真实Gateway联调待本轮补证。 | 并发与队列有上限，0.1秒有工作时检查；无需逐帧轮询认证，正文流式上限值得保留。 | WeakThis/终态Gate、析构CancelAll、凭据保护和拒绝自动重定向路径存在。 |
| [GamePlatformPCG](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformPCG/GamePlatformPCG.uplugin)（2模块） | 有界装饰请求与编辑器模板/Schema/节点基础；终态历史耗尽128请求容量；正式模板资产未交付 | 0.05秒遍历所有历史请求和订阅；清理使用原生同步CleanupLocalImmediate；四并发限制不能阻止128终身累计上限。 | Dedicated/Listen拒绝RuntimeCosmetic、Data租约、当前Region/World代次验证、清理原生输出后才释放。 涉及 GW-08。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformPresentation](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/GamePlatformPresentation.uplugin)（2模块） | D1。中立请求、LocalPlayer注册、上下文贡献、目录和预览舞台成立，平台未反向引用项目。P13排序/歧义仍有 F01/F02。Public接口和字段中文说明不完整。 | Contributor注册时排序，Submit无需复制排序；目录每次双层全扫，上限64片段×512项；没有语义索引/解析缓存。是热点候选，不是实测瓶颈。预览Actor无Tick。 | 按源码职责/端侧/中文说明核对；D1。中立请求、LocalPlayer注册、上下文贡献、目录和预览舞台成立，平台未反向引用项目。P13排序/歧义仍有 F01/F02。Public接口和字段中文说明不完整。 动态验收仍待证。 |
| [GamePlatformProgression](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/GamePlatformProgression.uplugin)（2模块） | 曲线/快照/缓存部分实现；事件与生产装配缺口 服务器中立资格端口、客户端账户代次、版本曲线兼容、嵌套ID索引/派生ViewModel缓存可见；无生产Configure、资格Provider或HTTP构造。 | 曲线二分O(logL)；快照变化重建View是O(NlogL)，查询ID平均O(1)；HTTP数组无上限；值返回复制，无测量。 | HTTP/缓存非权威基础方向正确；公开中文API大段缺失、README不满足最小接入/目标/配置/测试；Public HTTP/Token实现及TargetAllowList不足。 |
| [GamePlatformQuest](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/GamePlatformQuest.uplugin)（3模块） | 状态机/OwnerOnly复制/事件索引/单飞持久化/对账框架；对账失败与满队列会丢重放材料 | 每个变更事件重建全部活跃目标索引并生成/排序/复制全任务快照；不是每帧扫描，但高事件率/历史任务规模需测量。 | 权威事件RuntimeId、每任务去重、Completion/Reward标识、异步Port和跨世界弱回调存在。 涉及 GW-05、GW-06、GW-12。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformSave](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformSave/GamePlatformSave.uplugin)（1模块） | 本地非权威存档机制已实现；IO故障验收缺失 8MiB载荷、16在途、同槽busy、稳定ID、CRC、临时文件/Flush/备份、Provider迁移、GI代次、回调重入门禁齐备；取消为回调失效，后台IO本身仍可结束。 | 后台IO降低游戏线程阻塞，但APP-04读前无限容量；payload编码/复制也会放大16x8MiB峰值，需要测量。 | ClientOnly+Client/Editor、无内容/业务反依赖、中文契约和非权威边界较完整。 |
| [GamePlatformServer](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/GamePlatformServer.uplugin)（1模块） | 注册/就绪/心跳/排空与双端Gameplay准入桥接实现；准入HTTP关闭生命周期和响应内存上限存在缺口。 | 心跳有间隔和重试，准入响应完成后才检查32KiB；并发登录压力与容量准入需实测。 | 模块启动只注册机制、不连生产；目标/Boot/SessionEpoch校验及终态门禁存在，控制面HTTP使用流式上限。 |
| [GamePlatformSession](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/OnlineServices/GamePlatformSession/GamePlatformSession.uplugin)（1模块） | 会话转移状态机、票据握手、Binding/事实一致性、失败与重连原语存在；端到端正式世界仍依赖未完成装配。 | 单活动转移，0.1秒截止/握手检查；首本地Controller等待不是全Actor扫描，实际网络时延与超时需测试。 | 票据不进URL/日志；世界弱指针、代次、取消/解绑、失败事实存在；原生核心通过不证明Travel成功。 |
| [GamePlatformSettings](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformSettings/GamePlatformSettings.uplugin)（4模块） | 设备核心与四模块框架已实现；无生产Descriptor Provider 分层解析、单一真源边界、敏感/端侧校验、用户上下文、迁移回滚、设备暂存/预览/确认已具备；当前无非测试IGamePlatformSettingsProvider，Server/Runtime不能称产品设置已接通。 | 0业务Tick，Snapshot查TMap；APP-14同步档案Load，UGameUserSettings Apply/Save同步路径；未测量。 | 模块/描述target限制、Runtime中立方向、敏感配置约束与中文说明较好；本轮门禁退出0且生产Provider缺失警告。 |
| [GamePlatformSFX](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/GamePlatformSFX.uplugin)（1模块） | D1。0.2.0；ClientOnly且Client/Editor白名单、游戏世界/非Dedicated/非Commandlet过滤、Data Definition租约、AudioFinished清理成立。P2 F06/F07；终态接口不足F08。产品Target/项目组合未启用SFX，项目无真实音效Definition/Sound。 | 无Tick；每声音独立AcquireDefinition，完成时线性遍历最多256名义实例找Component，始终Spawn/Destroy而非自建池；重度5v5需Audio Insights/并发虚拟化验证。 | 按源码职责/端侧/中文说明核对；D1。0.2.0；ClientOnly且Client/Editor白名单、游戏世界/非Dedicated/非Commandlet过滤、Data Definition租约、AudioFinished清理成立。P2 F06/F07；终态接口不足F08。产品Target/项目组合未启用SFX，项目无真实音效Definition/Sound。 动态验收仍待证。 |
| [GamePlatformSurface](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/GamePlatformSurface.uplugin)（2模块） | D1。状态验证/裁剪/事件/MPC桥完整，Client+Editor端侧正确；F09默认资源链缺失，F10同步加载。没有Content目录，MPC、9个必需母材质/函数均只有路径合同；无项目C++消费者。 | 无Tick；状态变化写固定8参数，重复状态去重。首次绑定在游戏线程LoadSynchronous，缺MPC时每次更新会重试加载。没有Shader/材质/设备成本证据。 | 按源码职责/端侧/中文说明核对；D1。状态验证/裁剪/事件/MPC桥完整，Client+Editor端侧正确；F09默认资源链缺失，F10同步加载。没有Content目录，MPC、9个必需母材质/函数均只有路径合同；无项目C++消费者。 动态验收仍待证。 |
| [GamePlatformTelemetry](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/GamePlatformTelemetry.uplugin)（1模块） | Schema/隐私/采样/有界缓冲/分批/重试存在；HTTP持有闭环与Public实现暴露需整改。 | 缓冲计数/字节上限、最多64条Metric合并、头索引摊销与8并发/有界重试存在；响应无接收上限，请求自持有风险未实测。 | 不影响权威动作，账号切换丢弃旧队列、不可变上下文共享、锁外回调、关停预算存在。 |
| [GamePlatformUI](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformUI/GamePlatformUI.uplugin)（1模块） | D2。CommonUI层栈、LocalPlayer、ViewModel事件及普通软资源句柄可用，存在真实项目UI消费者。F11失活过早撤销资源需求/重入时序；不将Widget直接HTTP或业务Tick指控到平台。 | 世界投影按需30Hz遍历最多128Widget，有池复用；通知/反馈低频事件定时器；池按类线性查找。无实际设备/复杂Widget布局性能测量。 | 按源码职责/端侧/中文说明核对；D2。CommonUI层栈、LocalPlayer、ViewModel事件及普通软资源句柄可用，存在真实项目UI消费者。F11失活过早撤销资源需求/重入时序；不将Widget直接HTTP或业务Tick指控到平台。 动态验收仍待证。 |
| [GamePlatformVFX](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/GamePlatformVFX.uplugin)（2模块） | D1。0.2.0；私有世界执行、World共享Definition缓存、Data租约、实例反向映射、Niagara原生池、取消/子定时器所有权已存在。真实Niagara/Definition/ReviewMap为0，SourceArt=6 PNG+shader源；P13兼容解析排序F03、复合依赖F04、未用回退F05。VFX→Core描述漏依赖由父代理实际架构门禁另证。 | 标准Play直接DefinitionId，热路径复用World缓存，不重复语义解析；兼容Resolver全目录扫描但有1024结果缓存；结束按Component map定位；LRU淘汰线性扫缓存。池策略有实际AutoRelease，但需真实Niagara取消/复用/GC留存验证。没有Insights、GPU、设备预算数据。 | 按源码职责/端侧/中文说明核对；D1。0.2.0；私有世界执行、World共享Definition缓存、Data租约、实例反向映射、Niagara原生池、取消/子定时器所有权已存在。真实Niagara/Definition/ReviewMap为0，SourceArt=6 PNG+shader源；P13兼容解析排序F03、复合依赖F04、未用回退F05。VFX→Core描述漏依赖由父代理实际架构门禁另证。 动态验收仍待证。 |
| [GamePlatformVillage](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/GameModes/GamePlatformVillage/GamePlatformVillage.uplugin)（3模块） | 3模块空壳；无活动源码消费者；教学训练应优先组合既有插件 | 无运行机制可测；Village服务器角色不由此壳证明。 | README明确教学/训练是Village体验、不能从角色名推导专用机制。 涉及 GW-13、GW-12。 本轮UE/网络/资产边界未验收。 |
| [GamePlatformWorld](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformWorld/GamePlatformWorld.uplugin)（2模块） | 开发世界身份/租约/区域/流送/就绪源码；网络Session入口明确Unsupported | 0.1秒扫描区域/观察者/订阅；Observer线性查找与每观察者全区域查询，约O(O²+O×R)；GetReadiness还同步Refresh/流送采样。 | Game/PIE与命令行过滤、GUID上下文代次、跨World所有权、Data租约、持续就绪失效清理。 涉及 GW-10。 本轮UE/网络/资产边界未验收。 |

## MOBA层：2插件

| 插件（链接至实际描述） | 设计完善度／当前缺口 | 性能问题与待测成本 | 项目规范核对 |
| --- | --- | --- | --- |
| [GamePlatformArena](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/GamePlatformArena.uplugin)（5模块） | 五模式契约/数据/状态/复制UI/HTTP适配实现；票据准入无真实调用方、重连未恢复Pawn | 双队最多10玩家，阵容/比分重建为小型有界扫描；Result复制到每次重试并立即最多5次重试，无退避；需故障/网络测量。 | 1v1~5v5人数来自ModeSpec，Hero资格缺Provider拒绝、比赛事件去重有界、客户端服务器不互依。 涉及 GW-03、GW-04、GW-11、GW-12。 本轮UE/网络/资产边界未验收。 |
| [MobaPresentation](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/Presentation/MobaPresentation/MobaPresentation.uplugin)（2模块） | D1。17个中立语义、Combat/Arena事实适配、预测去重、世界退出解绑可见，无Niagara执行或项目反向引用。F12类型化上下文丢失、F13迟到Pawn漏绑定。 | 无Tick；每个Fact重新取Contributor Keys并排序；去重超过2048时RemoveAt(0)搬移；LatestAvatarGeneration长世界会话随实体增长，暂无数量/淘汰界限。需5v5高频事实和迟到复制联调。 | 按源码职责/端侧/中文说明核对；D1。17个中立语义、Combat/Arena事实适配、预测去重、世界退出解绑可见，无Niagara执行或项目反向引用。F12类型化上下文丢失、F13迟到Pawn漏绑定。 动态验收仍待证。 |

## 项目代码：5插件

| 插件（链接至实际描述） | 设计完善度／当前缺口 | 性能问题与待测成本 | 项目规范核对 |
| --- | --- | --- | --- |
| [DBAArena](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/DBAArena.uplugin)（3模块） | 五模式结构目录/可选客户端流程与服务器扩展；全部生产模式NotConfigured；多世界适配器共享缺陷 | 每Assignment预热12定义且全局重置句柄；Spawn按Controller迭代查玩家为最多10人的有界操作；UI按复制事件重建而无Tick。 | Server/Editor明确目标限制、不带DBAClient；模式未批准配置Fail Closed，公共流程由扩展接入不复制执行器。 涉及 GW-02、GW-04、GW-15、GW-16、GW-17。 本轮UE/网络/资产边界未验收。 |
| [DBAClient](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin)（5模块） | ApplicationFlow/Input/UI有真实C++装配；产品流程仍有Retry及输入资产接线缺口 复用唯一平台执行器、业务上下文只保留稳定值与瞬态ConnectionMaterial、真实Loading/Session六事实，UI LocalPlayer适配GI流程、页面事件与ViewModel命令，密码提交/失活清空；Input平台语义/GAS适配+合并刷新可见。默认Profile空且配置无赋值，当前Gameplay输入没有真实资产接线。 表现/外观：表现包协调D1、角色外观/预览D2。公共实现不依赖MOBA，外观使用平台薄软加载入口、请求代次取消、真实角色Ready事件；预览隔离旧卸载流送关卡并恢复原相机/Pawn可见性。F14激活伪事务及F15默认Definition不可执行；Client无Private/Tests，复杂生命周期仅有历史Manifest片段。 | DBAFlow多次SetBusy/SetError/Refresh...均广播并使UI Adapter复制全ViewState+Revision增，不做等值合并；RootLayout LoadSynchronous在初始化/状态失败重试时可能阻塞；Input无业务Tick且QueueActivationRefresh合并，当前未测。 表现：外观每角色最多约5秒20次状态查找重试；BeginPlay只扫一次角色；预览激活事件扫Actor选择舞台；每原型材质槽生成MID。无永久Tick，但首次大量角色加载/GC需测。 | 公共插件不依赖竞技、模块依赖方向较好；多数中文类责任具备但大量UI合同字段只靠名称缺范围/含义说明，Descriptor ClientOnly模块未列targets。登录源码事件驱动且密码瞬态路径可见，未用静态源码替代Monolith资产/焦点/可访问性验收。 另见表现F14/F15及服务器G-02。 |
| [DBAGameplay](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAGameplay/DBAGameplay.uplugin)（2模块） | Shared生成身份/十二生肖定义/角色初始化/Momentum扩展；基础ACharacter可Ready但无ASC | 角色事件/RepNotify驱动；同一RuntimeState版本变化会取消并重新申请定义，未见每帧资产冷加载；MomentumCOND_None复制需实测。 | 非竞技项目依赖不引MOBA；真实12个Definition文件；过期异步回调核对请求/Spawn/Avatar代次。 涉及 GW-02。 本轮UE/网络/资产边界未验收。 |
| [DBAServer](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAServer/DBAServer.uplugin)（1模块） | 三角色Profile、必要资产/BeginPlay验证、注册Ready/心跳/排空装配存在；世界退出后的Ready撤销路径需完善。 | 角色配置/资产就绪低频，心跳统计遍历当时玩家数；不重复进行全局分片分配。 | 唯一Server目标、OpenWorld PersistentShardedWorld/Village ExperienceInstance/MainArena PerMatch策略不同；只从服务器环境读取内部凭据。 |
| [DBAWorlds](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAWorlds/DBAWorlds.uplugin)（1模块） | 项目World Definition派生与正式角色/体验/模式一致性校验；不拥有地图运行能力 | 纯定义验证/小Shared目录查询，未发现高频热点；不能由定义文件证明地图/Cook可用。 | 三角色正式目录校验，Lobby落OpenWorld；非竞技世界携带ArenaMode拒绝。主代理另报Core/Data描述依赖缺失。  本轮UE/网络/资产边界未验收。 |

## 真实内容：16插件

| 插件（链接至实际描述） | 设计完善度／当前缺口 | 性能问题与待测成本 | 项目规范核对 |
| --- | --- | --- | --- |
| [DBAContentPack_Common](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Common/DBAContentPack_Common/DBAContentPack_Common.uplugin)（0模块） | D2原型共享源；131402435字节源包。具有明确所有者；Common内DBA/Standard同名母材质并存应通过AssetRegistry确认是否仍有合法消费者。共同H风险。 | 源文件字节不等于Cook后内存/GPU成本；需骨架/纹理LOD/动画授权及引用、干净Cook去冗余验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAFrontEndPack](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack/DBAFrontEndPack.uplugin)（0模块） | D2前端原型。与权威世界分离；平台舞台C++消费者存在；目前README仍说没有Client启动映射而配置已提供Custom FrontEndClient，应同步说明。 | 流送切入/切出、不同LocalPlayer舞台所有权、灯光与曝光、Cook引用需当前UE回读；没有场景GPU实测。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。  |
| [DBAHeroPack_Boar](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Boar/DBAHeroPack_Boar.uplugin)（0模块） | D2亥猪外观原型；Boar与当前消费者一致，未改旧身份；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Dog](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog/DBAHeroPack_Dog.uplugin)（0模块） | D2戌狗外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Dragon](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon/DBAHeroPack_Dragon.uplugin)（0模块） | D2辰龙外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Goat](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Goat/DBAHeroPack_Goat.uplugin)（0模块） | D2未羊外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Horse](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Horse/DBAHeroPack_Horse.uplugin)（0模块） | D2午马外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Monkey](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Monkey/DBAHeroPack_Monkey.uplugin)（0模块） | D2申猴外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Ox](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Ox/DBAHeroPack_Ox.uplugin)（0模块） | D2丑牛外观原型；非完整英雄内容；共同H风险 | 同上，不能用描述中“占位资源”证明资产可运行。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Rabbit](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rabbit/DBAHeroPack_Rabbit.uplugin)（0模块） | D2卯兔外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Rat](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rat/DBAHeroPack_Rat.uplugin)（0模块） | D2子鼠外观原型；非完整英雄内容；共同H风险 | 仅原型颜色；真实Mesh/骨架兼容、引用、材质参数、LOD、运行成本未回读。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Rooster](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rooster/DBAHeroPack_Rooster.uplugin)（0模块） | D2酉鸡外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Snake](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Snake/DBAHeroPack_Snake.uplugin)（0模块） | D2巳蛇外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAHeroPack_Tiger](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Tiger/DBAHeroPack_Tiger.uplugin)（0模块） | D2寅虎外观原型；非完整英雄内容；共同H风险 | 同上；Manifest中的历史Tiger预览不能替代当前MainArena运行。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。 涉及服务器G-02/F16。 |
| [DBAUIPack_Core](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/DBAUIPack_Core.uplugin)（0模块） | D2。合法项目UI资产归属与英文FriendlyName；Manifest记录Monolith0.20.3、历史编译/保存/回读和明确限制。当前Editor未运行，未读取Widget树/动态父类/焦点。 | 历史LOGO/PIE/Cook条目只证明当时范围；小窗口、移动、可访问性、真实键鼠、最新硬引用及服务器容器均待证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。  |
| [DBAWorldPack_Village](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/DBAWorldPack_Village.uplugin)（0模块） | D1首批流程地图与世界Definition。README:9还把Main/Training写后续，实际两Definition已在盘；最终湖心三岛PCG/世界美术未完成，不能称三体验内容验收通过。 | 地图内部Actor、导航、碰撞、权威PCG结果、出生安全点未在UE读取；三角色联机/地图Cook与服务器Ready须另证。 | 已登记纯内容包、英文FriendlyName；仅磁盘清单和历史Manifest，父类/引用/Cook未本轮回读。  |

## 逐项源码发现（根审查、玩法世界、应用与玩家服务）

以下编号用作整改追踪，同一规范问题涉及多个插件；不把重复诊断或交付缺口统计成多个独立运行故障。P1优先处理权威/生命周期或产品闭环阻断；P2修复具体正确性/资源/规范问题；P3改进文档及需测量的热点。源码路径与场景证据不等于已运行复现，各项验证边界分别说明。

### GW-01／P1：激活资格缺实现

涉及：GamePlatformAbilitySystem。

IGamePlatformAbilityActivationGate只声明，没有被生产ASC或技能基类消费。输入路径仅核对本地拥有者/代次，TryActivateAbility沿用原生条件；Inactive/排空等玩法资格不能阻止已授予能力激活。孤立AllowsInput策略测试不证明真实GAS门禁。

证据：[IGamePlatformAbilityActivationGate.h:14](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformAbilitySystem/Source/GamePlatformAbilitySystem/Public/Interfaces/IGamePlatformAbilityActivationGate.h:14)；[GamePlatformAbilitySystemComponent.cpp:190](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformAbilitySystem/Source/GamePlatformAbilitySystem/Private/Components/GamePlatformAbilitySystemComponent.cpp:190)；[GamePlatformAbilitySystemComponent.cpp:207](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformAbilitySystem/Source/GamePlatformAbilitySystem/Private/Components/GamePlatformAbilitySystemComponent.cpp:207)

### GW-02／P1：真实角色/GAS/资格未装配

涉及：GamePlatformGameplay、GamePlatformCharacter、GamePlatformAbilitySystem、GamePlatformCombat、DBAGameplay、DBAArena。

MainArena显式生成基础ACharacter，初始化器只加DivineBeastsCharacterComponent。没有ASC/Combat/ActorInfo接线；角色ApplyDefinition找不到ASC时仍返回成功，Ready可成立而Momentum/技能/Combat不存在。全活动C++的BindAbilityActorInfo和SetServerPlayerActive只有定义及测试调用。

证据：[DivineBeastsArenaGameplayLifecycleAdapter.cpp:162](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Private/Server/DivineBeastsArenaGameplayLifecycleAdapter.cpp:162)；[DivineBeastsCharacterSpawnInitializer.cpp:23](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsCharactersRuntime/Private/Initialization/DivineBeastsCharacterSpawnInitializer.cpp:23)；[DivineBeastsCharacterComponent.cpp:308](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsCharactersRuntime/Private/Components/DivineBeastsCharacterComponent.cpp:308)；[DivineBeastsCharacterComponent.cpp:326](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsCharactersRuntime/Private/Components/DivineBeastsCharacterComponent.cpp:326)

### GW-03／P1：竞技票据准入无调用方

涉及：GamePlatformArena。

ValidateTicketAndAdmit仅有声明/定义；ServerSubsystem只加载Assignment与提交Result，PlayerController只有选人/Ready/弃权RPC。当前源码没有真实连接入口消费票据并准入Roster，比赛无法自动凑齐已准入玩家。

证据：[GamePlatformArenaServerCoordinator.cpp:53](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArenaServer/Private/Server/GamePlatformArenaServerCoordinator.cpp:53)；[GamePlatformArenaServerSubsystem.cpp:86](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArenaServer/Private/Server/GamePlatformArenaServerSubsystem.cpp:86)；[GamePlatformArenaGameMode.cpp:247](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Private/Framework/GamePlatformArenaGameMode.cpp:247)

### GW-04／P1：重连不恢复受控角色

涉及：GamePlatformArena、DBAArena。

TryReconnectPlayer只恢复新PlayerState与统计并消耗重连Timer；没有重新Spawn/Possess/角色初始化。MainArena DefaultPawnClass被设空，新Controller即便准入成功也没有可控制Pawn；只依靠已安排死亡复活计时器不能覆盖活玩家断线重连。

证据：[GamePlatformArenaGameMode.cpp:535](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Private/Framework/GamePlatformArenaGameMode.cpp:535)；[GamePlatformArenaGameMode.cpp:550](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Private/Framework/GamePlatformArenaGameMode.cpp:550)；[DivineBeastsArenaServerProjectExtension.cpp:42](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Private/Server/DivineBeastsArenaServerProjectExtension.cpp:42)

### GW-05／P1：Quest对账丢事件

涉及：GamePlatformQuest。

HandleReconcileCompleted先GenerateValueArray并Reset PendingEventPayloads，随后ApplyLoadedSnapshots失败时丢失全部待重放payload；成功后EnqueueDeferredEvent的容量失败也被忽略，满队列时尚未持久化事件永久丢失。

证据：[GamePlatformQuestServerSubsystem.cpp:882](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp:882)；[GamePlatformQuestServerSubsystem.cpp:889](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp:889)；[GamePlatformQuestServerSubsystem.cpp:899](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp:899)；[GamePlatformQuestServerSubsystem.cpp:776](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp:776)

### GW-10／P1：交付缺口非伪成功

涉及：GamePlatformWorld。

InitializeSessionWorld始终Unsupported(SessionPrerequisiteMissing)。目前只存在显式Foundation开发初始化，正式网络世界会话/真实三维世界就绪链未完成；这是公开声明且正确拒绝的产品验收阻断。

证据：[GamePlatformWorldSubsystem.cpp:43](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformWorld/Source/GamePlatformWorld/Private/Subsystems/GamePlatformWorldSubsystem.cpp:43)

### GW-15／P1：多竞技世界共享适配器悬垂指针

涉及：DBAArena。

模块注册一个进程级ProjectExtension，扩展只持有一份shared GameplayLifecycleAdapter。第二个GameMode配置覆盖并释放前一份，前GameMode保留裸指针并在Spawn/Respawn解引用；前世界的复活计时器也随析构清空。预热句柄同样全局重置，违背每World所有权。

证据：[DivineBeastsArenaServer.cpp:12](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Private/DivineBeastsArenaServer.cpp:12)；[DivineBeastsArenaServerProjectExtension.h:36](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Public/Server/DivineBeastsArenaServerProjectExtension.h:36)；[DivineBeastsArenaServerProjectExtension.cpp:46](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Private/Server/DivineBeastsArenaServerProjectExtension.cpp:46)；[DivineBeastsArenaServerProjectExtension.cpp:59](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Private/Server/DivineBeastsArenaServerProjectExtension.cpp:59)；[GamePlatformArenaGameMode.h:109](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Public/Framework/GamePlatformArenaGameMode.h:109)；[GamePlatformArenaGameMode.cpp:347](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Private/Framework/GamePlatformArenaGameMode.cpp:347)

### GW-16／P1：五模式生产配置未交付

涉及：DBAArena。

五个正式模式都由MakeStructuralMode设置NotConfigured；客户端ValidateProduction拒绝匹配、服务器ProjectExtension也只接受ProductionSpec。五模式人数/结构存在，不等于五模式可运行；需要真实批准配置/地图资产与五模式联调。

证据：[DivineBeastsArenaModeCatalog.cpp:21](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaRuntime/Private/Catalog/DivineBeastsArenaModeCatalog.cpp:21)；[DivineBeastsArenaClientSubsystem.cpp:100](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/Private/Client/DivineBeastsArenaClientSubsystem.cpp:100)；[DivineBeastsArenaServerProjectExtension.cpp:19](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Private/Server/DivineBeastsArenaServerProjectExtension.cpp:19)

### R-03／P1：HTTP提供者关闭生命周期缺失

涉及：GamePlatformServer。

Admission HTTP完成委托捕获裸this并访问Requests；提供者没有析构/Shutdown取消和解绑所有请求。模块Shutdown直接Reset提供者，即使请求仍由HTTP系统持有也未解除回调，存在模块关停/热重载后的悬垂访问风险。子系统取消部分操作不能证明模块卸载时已清空所有请求。

证据：[GamePlatformHttpAdmissionProvider.cpp:210](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Source/GamePlatformServer/Private/Server/GamePlatformHttpAdmissionProvider.cpp:210)；[GamePlatformServer.cpp:61](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Source/GamePlatformServer/Private/GamePlatformServer.cpp:61)

验证边界：生命周期路径源码可证；尚未执行关停期间迟到HTTP回调复现。

### APP-01／P2：重置与重绑没有限制为当前Profile拥有的映射行

涉及：GamePlatformInput。

其他功能（菜单/车辆等）向同一个EnhancedInputUserSettings登记映射行后，ResetMappings(NAME_None)会枚举当前KeyProfile全部行并逐一ResetAllPlayerKeysInRow；ApplyRebind也只验证行存在，不验证本Profile所有权。与公开合同仅本配置/不能清其他功能冲突。

证据：[GamePlatformInputLocalPlayerSubsystem.cpp:1481](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp:1481)；[GamePlatformInputLocalPlayerSubsystem.cpp:1399](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp:1399)；[IGamePlatformInputService.h:50](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Public/Interfaces/IGamePlatformInputService.h:50)

整改方向：追踪Profile登记行的所有权，List/Preview/Apply/Reset只允许该集合；添加外部Context行不受重置影响的回归。

### APP-02／P2：未确认写盘结果就公布输入偏好已保存

涉及：GamePlatformInput。

SaveInputPreferences在Settings->SaveSettings及GConfig->Flush后无错误/完成检查，立即bPreferencesSaved=true并返回Success；只读文件、磁盘失败时内存状态和公开保存合同失真。

证据：[GamePlatformInputLocalPlayerSubsystem.cpp:1530](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp:1530)；[IGamePlatformInputService.h:55](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Public/Interfaces/IGamePlatformInputService.h:55)

整改方向：拆开受理与真实写盘终态，传播原生和INI持久化失败；测试失败后保存标志仍为false且键位内存仍生效。

### APP-03／P2：PIE输入偏好命名空间缺少实例身份

涉及：GamePlatformInput。

两个PIE实例使用同一LocalSettingsKey/Profile且ControllerId相同，StableSettingsKey的Namespace均为PIE，最终SettingsSection相同，保存会共享GGameUserSettingsIni区段。公开注释承诺PIE自动另加实例命名空间，当前实现仅区分PIE与Game。

证据：[GamePlatformInputLocalPlayerSubsystem.cpp:528](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp:528)；[GamePlatformInputLocalPlayerSubsystem.cpp:571](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp:571)；[GamePlatformInputLocalPlayerSubsystem.cpp:1532](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp:1532)；[IGamePlatformInputService.h:13](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Public/Interfaces/IGamePlatformInputService.h:13)

整改方向：PIE使用作用域/PIE实例身份或禁用真实持久化；验证两个GI同ControllerId的区段与写盘隔离。

### APP-04／P2：文件容量限制在全文件读入内存后才生效

涉及：GamePlatformSave。

ReadFile直接LoadFileToArray，没有预检FileSize；DecodeRecord才按8MiB载荷上限拒绝。巨大/损坏gpsav及bak在后台仍会分配全文件内存，16并发门禁不能限制单文件或总字节数。属于源码可推导内存风险，未进行测量。

证据：[GamePlatformSaveStorage.cpp:79](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformSave/Source/GamePlatformSaveClient/Private/Storage/GamePlatformSaveStorage.cpp:79)；[GamePlatformSavePolicy.cpp:13](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformSave/Source/GamePlatformSaveClient/Private/Policy/GamePlatformSavePolicy.cpp:13)；[GamePlatformSavePolicy.cpp:185](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformSave/Source/GamePlatformSaveClient/Private/Policy/GamePlatformSavePolicy.cpp:185)

整改方向：读前限制Envelope最大总字节，使用有界读取；补真实超大文件、备份超大文件、并发内存压力用例。

### APP-05／P2：订阅回调关闭服务后继续访问已释放Scope

涉及：GamePlatformLoading。

Loading订阅回调若触发GameInstance关闭/Deinitialize，Deinitialize立即Scope.Reset；Tick回调返回后仍++Scope->TotalSubscriberCallbacks，并随后写耗时。公开订阅合同未禁止关闭，调用前检查Scope不能保护回调返回后的访问。

证据：[GamePlatformLoadingSubsystem.cpp:88](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformLoading/Source/GamePlatformLoading/Private/Subsystems/GamePlatformLoadingSubsystem.cpp:88)；[GamePlatformLoadingSubsystem.cpp:372](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformLoading/Source/GamePlatformLoading/Private/Subsystems/GamePlatformLoadingSubsystem.cpp:372)；[GamePlatformLoadingSubsystem.cpp:384](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformLoading/Source/GamePlatformLoading/Private/Subsystems/GamePlatformLoadingSubsystem.cpp:384)

整改方向：在分发栈延后关闭或持有独立分发状态，回调返回后复查Scope/代次；加入订阅回调内关闭用例。

### APP-06／P2：状态广播可清空Transport，随后仍解引用

涉及：GamePlatformInventory、GamePlatformEntitlement、GamePlatformCommerceUI。

订阅OnChanged/OnCommerceStateChanged的消费者在Loading状态调用ResetAccount，源码先广播且无重入保护，ResetAccount清空Transport，广播返回后RefreshSnapshot/RefreshCatalog继续Transport->Begin...而未重新验证。Inventory、Entitlement和Commerce均有这一代码形态；通常现有UI只读不触发，但公开服务没有限制/防护该有效调用。

证据：[GamePlatformInventoryClientSubsystem.cpp:250](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Private/Services/GamePlatformInventoryClientSubsystem.cpp:250)；[GamePlatformInventoryClientSubsystem.cpp:190](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Private/Services/GamePlatformInventoryClientSubsystem.cpp:190)；[GamePlatformEntitlementClientSubsystem.cpp:59](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Private/Services/GamePlatformEntitlementClientSubsystem.cpp:59)；[GamePlatformEntitlementClientSubsystem.cpp:28](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Private/Services/GamePlatformEntitlementClientSubsystem.cpp:28)；[GamePlatformCommerceClientSubsystem.cpp:93](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Services/GamePlatformCommerceClientSubsystem.cpp:93)；[GamePlatformCommerceClientSubsystem.cpp:54](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Private/Services/GamePlatformCommerceClientSubsystem.cpp:54)

整改方向：广播栈禁止或延后控制变更，或快照Transport并验证账号/请求代次；回归覆盖状态监听器立即重置账号。

### APP-07／P2：装备服务器组件结束时不撤销GAS授予，也不失效持久化回调

涉及：GamePlatformEquipment。

服务器组件只有BindAvatar时撤销旧授予，无EndPlay/取消接口；异步Persistence回调仅WeakThis，没有世界结束/运行代次检查。组件EndPlay后UObject仍可存活，存活于PlayerState的ASC继续保留能力/效果，迟到回调也可再ApplyRuntimeSnapshot。当前未找到生产Provider，故不能宣称当前游戏已经发生该故障。

证据：[GamePlatformEquipmentServerComponent.h:20](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Components/GamePlatformEquipmentServerComponent.h:20)；[GamePlatformEquipmentServerComponent.h:73](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Components/GamePlatformEquipmentServerComponent.h:73)；[GamePlatformEquipmentServerComponent.cpp:47](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Private/Components/GamePlatformEquipmentServerComponent.cpp:47)；[GamePlatformEquipmentServerComponent.cpp:100](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Private/Components/GamePlatformEquipmentServerComponent.cpp:100)；[GamePlatformEquipmentServerComponent.cpp:449](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Private/Components/GamePlatformEquipmentServerComponent.cpp:449)；[GamePlatformEquipmentPersistencePort.h:16](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Interfaces/GamePlatformEquipmentPersistencePort.h:16)

整改方向：增加幂等运行关闭、回调代次、持久化取消和旧ASC授予清理；验证Pawn/PlayerState两类ASC寿命及EndPlay迟到响应。

### APP-08／P2：重复SlotId覆盖授予句柄，导致无法撤销的重复效果

涉及：GamePlatformEquipment。

ApplyRuntimeSnapshot不预检Slots唯一性，逐项Grant后NewHandles.Add(Slot.SlotId,Handle)覆盖同键旧Handle；重复SlotId会在ASC产生两次授予但只保留后一份句柄。Visual组件也以SlotId覆盖ActiveStaticMeshes，使先创建的组件失去清理索引。

证据：[GamePlatformEquipmentServerComponent.cpp:401](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Private/Components/GamePlatformEquipmentServerComponent.cpp:401)；[GamePlatformEquipmentVisualComponent.cpp:78](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentClient/Private/Components/GamePlatformEquipmentVisualComponent.cpp:78)；[GamePlatformEquipmentVisualComponent.cpp:242](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentClient/Private/Components/GamePlatformEquipmentVisualComponent.cpp:242)

整改方向：在任何撤销/授予前验证快照唯一SlotId、容量与身份，拒绝整份无效快照；覆盖重复槽位的GAS与视觉清理回归。

### APP-09／P2：首次快照、新轨道、清空与错误无可订阅状态事件

涉及：GamePlatformProgression。

公开事件只有OnXPChanged/OnLevelChanged。ApplySnapshot对新轨道!Old直接continue；ResetAccount清空与Loading/Error/Ready变化也不广播。因此事件驱动UI不能在首次快照、新轨道或账号重置时刷新，必须额外轮询或自建传输逻辑。

证据：[GamePlatformProgressionClientSubsystem.h:77](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Public/Services/GamePlatformProgressionClientSubsystem.h:77)；[GamePlatformProgressionClientSubsystem.cpp:31](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Services/GamePlatformProgressionClientSubsystem.cpp:31)；[GamePlatformProgressionClientSubsystem.cpp:238](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Services/GamePlatformProgressionClientSubsystem.cpp:238)；[GamePlatformProgressionClientSubsystem.cpp:296](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Services/GamePlatformProgressionClientSubsystem.cpp:296)

整改方向：提供只读Snapshot/State/Error变化事件；首次、增加、删除、错误和重置均发一致事件，并对旧账号响应不广播。

### APP-10／P2：账号重置清空玩家状态却不广播失效

涉及：GamePlatformLiveOps。

ResetAccount清空PlayerState/LastClaim但无OnPlayerStateChanged/OnClaimChanged；已有事件驱动Widget/ViewModel会保留前账号的签到/领取展示，直到新账号请求成功。账户代次阻止旧回调改写内部数据，但不能清空消费者缓存。

证据：[GamePlatformLiveOpsClientSubsystem.cpp:92](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Private/Services/GamePlatformLiveOpsClientSubsystem.cpp:92)；[GamePlatformLiveOpsClientSubsystem.h:85](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Public/Services/GamePlatformLiveOpsClientSubsystem.h:85)；[GamePlatformLiveOpsClientSubsystem.cpp:623](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Private/Services/GamePlatformLiveOpsClientSubsystem.cpp:623)

整改方向：重置时广播失效快照/统一状态事件，覆盖账号A显示->Logout/B加载失败期间不显示A状态。

### APP-11／P2：失败后的Retry被公布为允许命令，但适配器总拒绝

涉及：DBAClient。

Flow失败且无ActiveFlow时AllowedActions包含Retry；Adapter将其投影为Retry命令，UIViewModel也能提交。SubmitUICommand没有Retry分支，落入default bAccepted=false，返回UI.Command.Rejected。用户看到允许恢复但无实际恢复链。

证据：[DivineBeastsApplicationFlowSubsystem.cpp:2386](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsApplicationFlowClient/Private/DivineBeastsApplicationFlowSubsystem.cpp:2386)；[DivineBeastsApplicationUIAdapter.cpp:51](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Adapters/Application/DivineBeastsApplicationUIAdapter.cpp:51)；[DivineBeastsApplicationUIAdapter.cpp:307](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Adapters/Application/DivineBeastsApplicationUIAdapter.cpp:307)

整改方向：把Retry接到明确安全的流程重启/恢复入口，并限制允许状态；测试失败->Retry->新run/无重复业务操作。

### APP-12／P2：集合字段缺失或错类型被接受为空成功快照

涉及：GamePlatformEntitlement、GamePlatformProgression。

JsonToSnapshot读取entitlements/tracks失败时返回true；只要Revision/时间字段有效，畸形集合可替换为合法空Snapshot并把服务置Ready，吞掉结构错误。应明确区分可空数组与缺失/错误类型；当前无共享正式契约证明这些字段可省略。

证据：[GamePlatformEntitlementGatewayHttpTransport.cpp:179](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Private/Transport/GamePlatformEntitlementGatewayHttpTransport.cpp:179)；[GamePlatformProgressionGatewayHttpTransport.cpp:185](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Private/Transport/GamePlatformProgressionGatewayHttpTransport.cpp:185)

整改方向：按现行正式契约要求明确必填/可选；缺失/错类型返回InvalidResponse并保留旧只读快照，添加JSON负例。

### G-01／P2：插件依赖声明门禁失败

涉及：DBAWorlds、GamePlatformVFX、DBAArena。

六种Client/Server/Editor公共与竞技装配报18条重复诊断，对应4条独立缺失描述依赖：DBAWorlds→Core/Data，VFX→Core，DBAArena→Character。Build.cs已有真实模块依赖而.uplugin未同步；72项架构回归因此失败3项。

证据：[DBAWorlds.uplugin:12](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAWorlds/DBAWorlds.uplugin:12)；[GamePlatformVFX.uplugin:29](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/GamePlatformVFX.uplugin:29)；[DBAArena.uplugin:12](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/DBAArena.uplugin:12)

验证边界：本轮实际门禁失败，可复现。

### G-02／P2：服务器声明装配带入公共客户端

涉及：DBAClient、DBAServer、DBAContentPack_Common、DBAHeroPack_Boar、DBAHeroPack_Dog、DBAHeroPack_Dragon、DBAHeroPack_Goat、DBAHeroPack_Horse、DBAHeroPack_Monkey、DBAHeroPack_Ox、DBAHeroPack_Rabbit、DBAHeroPack_Rat、DBAHeroPack_Rooster、DBAHeroPack_Snake、DBAHeroPack_Tiger。

实际.uproject全目标启用12英雄/Common；英雄无目标白名单依赖DBAClient。合并Server Target根后，当前静态闭包确实包含DBAClient和DivineBeastsPresentationRuntime。DedicatedServer Stage规则不排除Common/Hero，不能据模块后缀或Target仅未显式Enable英雄宣告服务器表现剥离。应拆审权威最小资源与纯表现目录；尚无本轮Cook/Stage制品证据。

证据：[DivineBeastsArena.uproject:20](E:/poject/feebooz/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject:20)；[DBAHeroPack_Dragon.uplugin:13](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon/DBAHeroPack_Dragon.uplugin:13)；[DivineBeastsArenaServer.Target.cs:12](E:/poject/feebooz/DivineBeastsWorkspace/Game/Source/DivineBeastsArenaServer.Target.cs:12)

验证边界：源码声明闭包可证；不声称服务器已经打包全部客户端/VFX资产。

### G-04／P2：中文说明未达到全量门禁

涉及：GamePlatformDebug、GamePlatformDeveloperTools、GamePlatformTelemetry。

例如Telemetry Buffer、Debug Registry与DeveloperTools ValidationService大量公开入口缺逐项中文调用/错误/线程说明；文件职责文档与部分注释已存在，不能推导全量注释合规。自动ASCII/继承门禁不检查中文说明质量。各分区另列私有实现与Build.cs存量，需纳入人工整改清单。

证据：[GamePlatformValidationService.h:17](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformDeveloperTools/Source/GamePlatformDeveloperTools/Public/Validation/GamePlatformValidationService.h:17)；[GamePlatformDebugRegistry.h:17](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformDebug/Source/GamePlatformDebug/Public/Registry/GamePlatformDebugRegistry.h:17)

验证边界：抽检实际缺项；未统计全库未合规文件数。

### GW-06／P2：Quest重复事件对账循环风险

涉及：GamePlatformQuest。

DuplicateEvent触发Reconcile，加载快照不带持久已处理事件集合，ApplyLoadedSnapshots清空Recent去重，随后将全部pending事件重放。主代理检索当前Backend未发现Quest领域/端点实现，持久幂等返回语义尚无真实实现可补证；接口必须明确已处理EventId/权威快照与重放边界，否则在未来适配中存在重复加进度、反复冲突对账的风险。

证据：[GamePlatformQuestServerSubsystem.cpp:21](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp:21)；[GamePlatformQuestServerSubsystem.cpp:212](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp:212)；[GamePlatformQuestServerSubsystem.cpp:899](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp:899)

验证边界：协议未闭合风险；当前没有真实后端重复事件故障或联机复现证据。

### GW-07／P2：导航重复RequestId破坏生命周期

涉及：GamePlatformNavigation。

同世界使用相同RequestId启动第二个异步查询，AsyncRequests.Add覆盖旧Record/Completion但不取消旧查询和Timeout。旧完成被EngineQueryId比较丢弃；旧Timeout只比RequestId/WorldGeneration，会删除并Abort新查询。不能保证每个请求单次终态。

证据：[GamePlatformNavigationWorldSubsystem.cpp:306](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformNavigation/Source/GamePlatformNavigationServer/Private/Subsystems/GamePlatformNavigationWorldSubsystem.cpp:306)；[GamePlatformNavigationWorldSubsystem.cpp:383](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformNavigation/Source/GamePlatformNavigationServer/Private/Subsystems/GamePlatformNavigationWorldSubsystem.cpp:383)；[GamePlatformNavigationWorldSubsystem.cpp:400](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformNavigation/Source/GamePlatformNavigationServer/Private/Subsystems/GamePlatformNavigationWorldSubsystem.cpp:400)；[GamePlatformNavigationWorldSubsystem.cpp:1073](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformNavigation/Source/GamePlatformNavigationServer/Private/Subsystems/GamePlatformNavigationWorldSubsystem.cpp:1073)；[GamePlatformNavigationWorldSubsystem.cpp:1114](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformNavigation/Source/GamePlatformNavigationServer/Private/Subsystems/GamePlatformNavigationWorldSubsystem.cpp:1114)

### GW-08／P2：PCG终身128请求容量

涉及：GamePlatformPCG。

Cleaned记录永不移除，ReleaseGeneration只启动清理。RequestGeneration按Requests.Num>=128拒绝，即使全部输出已清理，常驻UWorld第129次装饰请求永久失败；0.05秒Ticker仍遍历所有终态历史。

证据：[GamePlatformPCGWorldSubsystem.cpp:97](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCG/Private/Subsystems/GamePlatformPCGWorldSubsystem.cpp:97)；[GamePlatformPCGWorldSubsystem.cpp:129](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCG/Private/Subsystems/GamePlatformPCGWorldSubsystem.cpp:129)；[GamePlatformPCGWorldSubsystem.cpp:293](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCG/Private/Subsystems/GamePlatformPCGWorldSubsystem.cpp:293)；[GamePlatformPCGWorldSubsystem.cpp:306](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCG/Private/Subsystems/GamePlatformPCGWorldSubsystem.cpp:306)

### GW-09／P2：Custom交互回调重入悬垂Option

涉及：GamePlatformInteraction。

CommitCurrentSession持有Options数组元素裸指针并传入Custom CommitInteraction；公开SetOptions允许回调替换数组并取消当前会话。Commit返回后仍读取旧Option->CommitKind，且没有重新核对Session代次/状态，存在悬垂读取与取消后再完成风险。

证据：[GamePlatformInteractorComponent.cpp:1123](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformInteraction/Source/GamePlatformInteraction/Private/Components/GamePlatformInteractorComponent.cpp:1123)；[GamePlatformInteractorComponent.cpp:1141](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformInteraction/Source/GamePlatformInteraction/Private/Components/GamePlatformInteractorComponent.cpp:1141)；[GamePlatformInteractorComponent.cpp:1165](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformInteraction/Source/GamePlatformInteraction/Private/Components/GamePlatformInteractorComponent.cpp:1165)；[GamePlatformInteractableComponent.cpp:156](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformInteraction/Source/GamePlatformInteraction/Private/Components/GamePlatformInteractableComponent.cpp:156)；[GamePlatformInteractableComponent.cpp:390](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformInteraction/Source/GamePlatformInteraction/Private/Components/GamePlatformInteractableComponent.cpp:390)

### GW-11／P2：稳定同领域框架未下继承

涉及：GamePlatformGameplay、GamePlatformArena。

MOBA GameMode/GameState/PlayerState/PlayerController直接继承UE类，GamePlatformGameplay已有稳定同领域平台基类却未复用，竞技独立处理准入/出生/玩家阶段，造成两套生命周期与资格接线分离；不违反依赖反向但不满足现行优先下继承规范。

证据：[GamePlatformArenaGameMode.h:20](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Public/Framework/GamePlatformArenaGameMode.h:20)；[GamePlatformArenaGameState.h:27](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Public/Framework/GamePlatformArenaGameState.h:27)；[GamePlatformArenaPlayerState.h:17](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Public/Framework/GamePlatformArenaPlayerState.h:17)；[GamePlatformArenaPlayerController.h:11](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Public/Framework/GamePlatformArenaPlayerController.h:11)

### GW-12／P2：端侧与中文说明门禁

涉及：GamePlatformAI、GamePlatformQuest、GamePlatformAnimation、GamePlatformNavigation、GamePlatformLobby、GamePlatformVillage、GamePlatformArena。

这些ClientOnly/ServerOnly描述未声明模块TargetAllowList，不能仅凭宿主名称证明普通Game目标已审计隔离。QuestServerSubsystem/NavWorldSubsystem公开API及多个Build.cs/模块入口没有完整中文线程/所有权/失败说明；当前不能声明全量注释规范通过。

证据：[GamePlatformQuest.uplugin:23](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/GamePlatformQuest.uplugin:23)；[GamePlatformNavigation.uplugin:24](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/World/GamePlatformNavigation/GamePlatformNavigation.uplugin:24)；[GamePlatformArena.uplugin:26](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/GamePlatformArena/GamePlatformArena.uplugin:26)；[GamePlatformQuestServerSubsystem.h:20](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Public/Subsystems/GamePlatformQuestServerSubsystem.h:20)

### GW-17／P2：离开竞技世界后旧UI状态残留

涉及：DBAArena。

RefreshArenaViewFromWorld遇到非竞技/尚无GameState仅Unbind后返回，未清空竞技ViewModel。ResolvePrimaryArenaSurfaceId仍按旧FlowState选HUD/结算，世界迁移期间可显示上一场身份/比分；初始未找到GameState后也缺少明确世界就绪重试订阅。

证据：[DivineBeastsArenaUIClientSubsystem.cpp:59](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/Private/Client/DivineBeastsArenaUIClientSubsystem.cpp:59)；[DivineBeastsArenaUIClientSubsystem.cpp:92](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/Private/Client/DivineBeastsArenaUIClientSubsystem.cpp:92)；[DivineBeastsArenaUIClientSubsystem.cpp:273](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/Private/Client/DivineBeastsArenaUIClientSubsystem.cpp:273)

### R-01／P2：长期内存增长

涉及：GamePlatformData。

每次ReleaseRequest把整份Lease（含Bundles）追加到ReleasedLeases，整个GameInstance内没有淘汰/压缩。活动资产虽能释放，幂等历史按累计请求数永久增长，长时间运行/频繁UI和特效加载会持续积累内存。

证据：[GamePlatformDataSubsystem.cpp:58](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData/Private/Subsystems/GamePlatformDataSubsystem.cpp:58)；[GamePlatformDataSubsystem.cpp:281](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData/Private/Subsystems/GamePlatformDataSubsystem.cpp:281)

验证边界：源码可确认增长；未量化实际字节/小时。

### R-02／P2：逐帧扫描热点

涉及：GamePlatformData。

OwnerWatch以未设置间隔的CoreTicker执行，每轮遍历全部请求并检测Caller/World；已成功但仍持有租约也参与。当前没有活跃租约上限或失效事件索引，成本随租约数线性增长。应保留正确释放语义，增加事件驱动清理或经过测量的周期/分批策略。

证据：[GamePlatformDataSubsystem.cpp:83](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData/Private/Subsystems/GamePlatformDataSubsystem.cpp:83)；[GamePlatformDataSubsystem.cpp:362](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData/Private/Subsystems/GamePlatformDataSubsystem.cpp:362)

验证边界：静态复杂度热点，不是已实测帧率瓶颈。

### R-04／P2：响应内存上限滞后

涉及：GamePlatformServer。

Admission使用默认响应缓冲，完成后调用GetContent才检查32KiB。超大响应已先分配/接收，因此32KiB不是传输接收内存上限；同插件ControlProvider已有流式限额，可沿用单一策略。

证据：[GamePlatformHttpAdmissionProvider.cpp:240](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Source/GamePlatformServer/Private/Server/GamePlatformHttpAdmissionProvider.cpp:240)

验证边界：源码可确认限制时点；未做大正文压测。

### R-05／P2：HTTP持有与接收预算风险

涉及：GamePlatformTelemetry。

Request的完成委托强捕获同一个RequestPtr，形成Request→Delegate→Request持有环；完成/失败/CancelAll均未主动Unbind，依赖引擎隐含清理。MaxPayloadBytes限制发送正文，响应接收没有流式限额。可能持续保留请求或大响应，应以锁定HTTP实现和故障/取消用例确认释放。

证据：[GamePlatformTelemetryTransport.cpp:261](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Source/GamePlatformTelemetry/Private/Transport/GamePlatformTelemetryTransport.cpp:261)；[GamePlatformTelemetryTransport.cpp:332](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Source/GamePlatformTelemetry/Private/Transport/GamePlatformTelemetryTransport.cpp:332)

验证边界：持有图与缺接收限额可确认；未声称已经测得实际泄漏。

### R-06／P2：Public实现边界与中文API说明

涉及：GamePlatformTelemetry。

Public/Buffer公开具体有界缓存实现，Public/Schema公开注册表实现，且Buffer的多数公开入口没有中文参数/失败/线程合同。现行规则要求缓存/注册表策略留Private；需保留必要稳定Schema数据及扩展接口，先核对外部消费者再收窄实现暴露。

证据：[GamePlatformTelemetryBoundedBuffer.h:6](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Source/GamePlatformTelemetry/Public/Buffer/GamePlatformTelemetryBoundedBuffer.h:6)；[GamePlatformTelemetrySchemaRegistry.h:47](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Source/GamePlatformTelemetry/Public/Schema/GamePlatformTelemetrySchemaRegistry.h:47)

验证边界：现行规范直接对应；迁移需先影响检查，不擅改公开身份。

### R-07／P2：性能验收能力未装配

涉及：GamePlatformDeveloperTools。

PerformanceExecutorRegistry存在注册入口，但活动Game源码检索只有定义/测试，未找到生产性能场景执行器。RunProfile会正确拒绝缺执行器；现阶段无法通过该工具生成真实场景CPU/GPU/内存证据。

证据：[GamePlatformPerformanceTestRunner.cpp:12](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformDeveloperTools/Source/GamePlatformDeveloperTools/Private/Performance/GamePlatformPerformanceTestRunner.cpp:12)；[GamePlatformPerformanceTestRunner.cpp:51](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Diagnostics/GamePlatformDeveloperTools/Source/GamePlatformDeveloperTools/Private/Performance/GamePlatformPerformanceTestRunner.cpp:51)

验证边界：交付缺口，拒绝路径是合理设计。

### R-08／P2：世界就绪所有权未闭合

涉及：DBAServer。

首次世界验证后bWorldValidated一直为true；观察新World不重置该标记，未订阅WorldCleanup撤销Ready/准入/心跳。旧World退出而GameInstance仍存活时，后续World BeginPlay被短路，注册就绪状态可能继续代表旧世界。应按角色策略明确禁止Travel或实现退出后撤销/重验。

证据：[DivineBeastsServerBootstrapSubsystem.cpp:166](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAServer/Source/DBAServer/Private/Server/DivineBeastsServerBootstrapSubsystem.cpp:166)；[DivineBeastsServerBootstrapSubsystem.cpp:286](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAServer/Source/DBAServer/Private/Server/DivineBeastsServerBootstrapSubsystem.cpp:286)；[DivineBeastsServerBootstrapSubsystem.cpp:370](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAServer/Source/DBAServer/Private/Server/DivineBeastsServerBootstrapSubsystem.cpp:370)

验证边界：同GameInstance世界更替场景风险，非正常同实例区域流送故障。

### APP-13／P3：单槽邮箱没有覆盖游戏线程唤醒队列的重复投递

涉及：GamePlatformApplicationFlow。

核心邮箱只接首次结果，但Adapter每次Completion（含迟到/重复）都创建AsyncTask唤醒GameThread；没有跨线程coalescing标志。故不能从邮箱有界推断TaskGraph排队有界，错误Provider批量完成可造成无界唤醒负担；无实测瓶颈结论。

证据：[ApplicationFlowExecutor.cpp:203](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/Source/GamePlatformApplicationFlow/Private/Execution/ApplicationFlowExecutor.cpp:203)；[GamePlatformApplicationFlowSubsystem.cpp:100](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/Source/GamePlatformApplicationFlow/Private/Subsystems/GamePlatformApplicationFlowSubsystem.cpp:100)

整改方向：完成受理结果参与唤醒决策，或原子合并一次待唤醒；压测重复/迟到Completion且观测待唤醒数。

### APP-14／P3：用户档案加载在游戏线程同步进行

涉及：GamePlatformSettings。

Load直接在游戏线程DoesSaveGameExist/LoadGameFromSlot，Reload/SwitchUserContext沿该路径读取档案；当前Provider无生产SettingId，但接线后登录/切换账号/重载可能阻塞。BeginSave为异步不能代替读取性能。

证据：[GamePlatformSettingsClientPersistenceProvider.cpp:86](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsClient/Private/Persistence/GamePlatformSettingsClientPersistenceProvider.cpp:86)；[GamePlatformSettingsSubsystem.cpp:569](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Application/GamePlatformSettings/Source/GamePlatformSettingsRuntime/Private/Subsystems/GamePlatformSettingsSubsystem.cpp:569)

整改方向：声明可接受加载预算并测真实档案尺寸/慢盘，或采用异步读与代次原子发布。

### APP-15／P3：玩家服务公开API/字段缺少完整中文合同

涉及：GamePlatformInventory、GamePlatformEquipment、GamePlatformEntitlement、GamePlatformProgression、GamePlatformLiveOps、GamePlatformCommerceUI。

Inventory虽有职责注释，RequestMove/Split/Merge/Quickbar等仍缺输入范围、失败与线程合同；Equipment、Entitlement、Progression、LiveOps、Commerce的核心Public接口大段无中文职责/参数/终态说明，Build.cs多无文件责任说明。LiveOps、Progression无Docs目录且README仅一段基线链接。自动ASCII/头检查通过不能证明注释内容合规。

证据：[GamePlatformInventoryClientSubsystem.cpp:82](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory/Source/GamePlatformInventoryClient/Private/Services/GamePlatformInventoryClientSubsystem.cpp:82)；[GamePlatformEquipmentServerComponent.h:23](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Components/GamePlatformEquipmentServerComponent.h:23)；[GamePlatformEquipmentPersistencePort.h:21](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment/Source/GamePlatformEquipmentServer/Public/Interfaces/GamePlatformEquipmentPersistencePort.h:21)；[GamePlatformEntitlementClientSubsystem.h:20](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement/Source/GamePlatformEntitlementClient/Public/Services/GamePlatformEntitlementClientSubsystem.h:20)；[GamePlatformProgressionClientSubsystem.h:32](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformProgression/Source/GamePlatformProgressionClient/Public/Services/GamePlatformProgressionClientSubsystem.h:32)；[GamePlatformLiveOpsClientSubsystem.h:23](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformLiveOps/Source/GamePlatformLiveOpsClient/Public/Services/GamePlatformLiveOpsClientSubsystem.h:23)；[GamePlatformCommerceClientSubsystem.h:23](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Source/GamePlatformCommerceUIClient/Public/Services/GamePlatformCommerceClientSubsystem.h:23)

整改方向：补文件责任、线程、账号与网络权威、单位/范围、异步/失败/取消说明和真实最小接线/测试文档；人工内容审核。

### G-03／P3：正式文档与当前源码失配

涉及：跨工程规则与文档。

总体规划仍称.uproject为Foundation最小装配；插件规范附录仍写内容N=1、DBAClient四模块与规划Pig，实际已16内容、DBAClient五模块及Boar。独立《整合与交付说明》未检索到；三层实施规划只提供历史整合证据。失真说明不满足同步工程文档要求。

证据：[解决方案总体规划.md:467](E:/poject/feebooz/DivineBeastsWorkspace/Docs/Architecture/解决方案总体规划.md:467)；[插件开发规范.md:855](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/插件开发规范.md:855)

验证边界：当前文件/描述盘点可证，历史执行记录原样保留不算伪造。

### GW-13／P3：空壳保留决策待收敛

涉及：GamePlatformAnimation、GamePlatformLobby、GamePlatformVillage。

Animation 2模块与Lobby/Village各3模块仅IMPLEMENT_MODULE、Core依赖，无Public契约/测试/运行消费者。README已诚实标为未完成/条件保留；保留稳定身份是现行文档授权例外，不能直接按空壳规则擅删，但不能计入已实现能力。

证据：[GamePlatformAnimation.cpp:3](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformAnimation/Source/GamePlatformAnimation/Private/GamePlatformAnimation.cpp:3)；[GamePlatformLobby.cpp:3](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/GameModes/GamePlatformLobby/Source/GamePlatformLobby/Private/GamePlatformLobby.cpp:3)；[GamePlatformVillage.cpp:3](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/GameModes/GamePlatformVillage/Source/GamePlatformVillage/Private/GamePlatformVillage.cpp:3)

### GW-14／P3：Gameplay文档职责失真

涉及：GamePlatformGameplay。

README与Docs/Architecture仍宣称仅最小Eligibility且不拥有出生/重生，但当前GameModeBase存在完整体验/准入/出生/排空实现及0.05秒轮询；职责文档落后于源码。

证据：[Architecture.md:3](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformGameplay/Docs/Architecture.md:3)；[GamePlatformGameModeBase.cpp:678](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformGameplay/Source/GamePlatformGameplay/Private/Framework/GamePlatformGameModeBase.cpp:678)；[GamePlatformGameModeBase.cpp:1043](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Gameplay/GamePlatformGameplay/Source/GamePlatformGameplay/Private/Framework/GamePlatformGameModeBase.cpp:1043)

## 表现与内容专项发现（F01—F17）

服务器F16与G-02属于同一问题，中文说明F17与G-04/APP-15/GW-12有交集；合并整改时应按责任与场景去重。

## 有依据的发现

以下为源码可复核问题。没有把缺少性能数据判为实测瓶颈。跨插件漏依赖由主代理实际门禁在G-01（P2）另列；本组确定行为缺陷主要为P2，文档/注释为P3。

### F01 / P2 平台表现目录先比作用域，父语义可覆盖精确语义

文件：`Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/GamePlatformPresentationClientSubsystem.cpp`，130先比Scope、134后比SemanticRank；365直接允许所有父标签，无显式允许开关。可复现配置：Project条目精确Semantic=Moba.Combat.Hit，ContentPack条目父Semantic=Moba.Combat；二者上下文合格，后者Scope3压过前者Scope2。P13要求精确语义第一、当前语义无合格结果且显式允许才逐级父回退。现有CatalogResolution测试只分别覆盖同语义Scope和单父回退，不覆盖二者冲突。

### F02 / P2 跨目录同EntryId绕过歧义

同文件392只在BestEntry.EntryId != Entry.EntryId时标Ambiguous；RegisterCatalogFragment只保证FragmentId唯一，Fragment.IsValid只保证片段内部EntryId唯一。两个不同Fragment可各用EntryId=Default、同排序键、不同DefinitionId，结果保留第一项，受注册顺序影响。应比较完整目录/条目身份，同键多结果不可用EntryId同名豁免；P13要求完全同键多结果失败并显示Pack/Catalog/Entry。

### F03 / P2 VFX兼容Resolver先比具体度，违反同语义Scope优先

文件：`Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Resolution/GamePlatformVFXCatalogRegistry.cpp`，30-32按ContextTier→Specificity→Scope；170又把RequiredContextTags.Num加入Specificity。同精确语义下，低层更多标签条目能压过高层包默认，且具体度没有按六个已验证等值项计算。标准Gameplay当前绕过此兼容Resolver，但公开低层Catalog及StartupCatalogs仍可调用，不可宣称两套规则一致。

### F04 / P2 Composite Steps可绕过数据依赖无环/预加载门禁

文件：`.../GamePlatformVFXClient/Public/Definitions/GamePlatformVFXCompositeDefinition.h:14` Step仅FName；`.../Private/Definitions/GamePlatformVFXCompositeDefinition.cpp:29` 至51只查直接自引用；Editor `.../GamePlatformVFXCompositeValidator.cpp:39` 声称间接环由Data阻断。实际Data唯一边源是 `Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData/Private/Subsystems/GamePlatformDataSubsystem.cpp:240` 的Definition.RequiredDefinitions；不存在Steps与RequiredDefinitions一致性校验。A.Steps=B、B.Steps=A、二者RequiredDefinitions空均能通过定义校验，运行会产生多代子请求到深度截断，而不是加载/注册时拒绝环；缺子Definition也是运行后才部分失败。MaxChildren只限制每个节点Steps，未证明根树总子数预算。

### F05 / P2 配置FallbackDefinition没有运行消费者

文件：`.../GamePlatformVFXClient/Public/Definitions/GamePlatformVFXDefinition.h:81` 宣告FallbackDefinition；全文搜索运行层无读取此字段。WorldSubsystem HandleCachedDefinitionLoaded:375失败直接Cleanup；NiagaraExecutor:21资源不存在直接nullptr，ExecuteLoadedDefinition:1154失败退出。可选变体或基础Niagara缺失无法按定义回退，关键预警可能完全消失。当前无真实Definition，所以尚未触发实际游戏事件；这是公开合同和未来内容接入的实质缺口。

### F06 / P2 SFX Pending与Active预算可以突破名义上限

文件：`Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp:108` 分别限制Pending128和Active256。255 Active时可以接受128 Pending；完成回调326-494未重查总额，最多升到383 Active。真实超限路径由源码即可确定；未将383声称实测。应按预留实例的总量或每类独立且明确的预算计数，补批量完成回归。

### F07 / P2 SFX起播后才建立完成监听，极短音源时序需真实复核

同文件400/412/434先SpawnSound*，482才AddUObject AudioFinished。如果声音立即结束/起播尾部/并发拒绝在这段窗口终止，平台不会收到终态，Active记录和租约没有定时/失败清扫备援。文件源码证明监听晚于起播，是否能在锁定引擎产生该窗口仍需极短SoundWave/MetaSound实测；本机安装目录没有AudioComponent.cpp，不能伪称引擎内部时序已证。SFX TestingAndEvidence.md已主动列该未完成测试。

### F08 / P2 Queued之后终态对调用者不可区分

SFX服务公开接口只有Play/Stop/IsActive/更新/Diagnostics（IGamePlatformSFXService.h:20起），FailPending:520仅日志后释放；VFX服务公开接口同样无终态查询/完成委托（GamePlatformVFXService.h:22起），Definition失败统一Cleanup。调用方只能看到Queued→不活动，无法区分完成、取消、失败、内容撤销和世界销毁。现有失败路径真实清理，不是固定成功，但不满足P16的可诊断终态契约。Presentation Provider也忽略Play受理结果：VFXProvider:71、SFXBridge:75返回true，平台可能记录Submitted而实际加载未受理。

### F09 / P2 Surface被产品启用但默认材质资源链不存在

Settings.cpp:7默认指向`/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal`；ClientTarget显式EnableSurface。实际范围没有Content目录或任何MPC/Material/Function，AssetContract.cpp:39起仅列9必需路径，EditorLibrary只生成MPC而不生成材质。默认Initialize只会记录绑定失败，状态更新不产生可见表面；应将现状明确为机制切片，真实链路补齐后验收。README25已诚实声明资产未制作，不算伪报。

### F10 / P2 Surface首次/失败后每次更新同步加载

SurfaceWorldSubsystem.cpp:158 在Initialize或Apply/Refresh时LoadSynchronous；失配且BoundCollection为空会每次重试。冷加载会阻塞游戏线程，缺失资源时增加重复查询。固定8参数事件桥成本清楚；这里只指出阻塞点，未测时延，后续应使用Data普通软资源入口异步持有句柄或明确预热合同。另83只看BoundInstance有效即返回Unchanged，之前ParameterContractMismatch未保持失败状态；重复输入会从失配变成“无变化”，需错误重试回归。

### F11 / P2 UI失活即撤销资源及关闭通知早于完整解绑

UIManagerSubsystem.cpp:765 AddWidget可能激活/失活页面，784才绑定Deactivated、790才转移ActiveScreenLeases；页面初始化事件中立刻关闭可漏掉清理后留下lease。818在任何Deactivate即移除lease，CommonUI覆盖但仍留栈的旧页会失去PreloadAssets需求，返回重激活没有加载路径。UIScreen.cpp:43先广播PlatformDeactivated，44才调用父类；ActivatableWidgetBase.cpp:82才EndPage。关闭观察者重入打开相同VM时，旧EndPage可结束新代次。现有项目主页面每次建立新VM降低重入概率，不能据此认为公开平台合同安全。需要真实栈覆盖/返回、初始化自闭、GC后软预载及回调重入测试。

### F12 / P2 MOBA到平台的类型化上下文丢失

MobaPresentationRequestBuilder.cpp:7-34从Fact复制空间及通用Request字段，没有给Request.Context赋任何值；MOBAContext已有HeroDefinitionId/AbilityId/ArenaModeId/AvatarGeneration（Types.h74/75/67/92），FromAbilityFact:189实际填入AbilityId。到平台目录Hero/Ability/Arena约束全部失效，项目默认ContextContributor可能补成本地英雄而非事实源英雄，旁观其他英雄时选择错映射。旧单测只查通用Request.WorldGeneration，不查类型化Context。

### F13 / P2 MOBA自动绑定漏迟到Possess及重生

MobaPresentationClientSubsystem.cpp:29/91/103为RefreshBindings唯一内部调用，分别初始化/世界重置/PostLoadMap；144读当前Controller/Pawn/Combat，缺失就不订阅。没有PlayerControllerChanged/PawnChanged/组件就绪订阅，项目范围搜索无外部RefreshBindings调用。同地图后续Possess、网络迟到Pawn、重生换Pawn都不会绑定新的Combat，无法自动适配Hit/Heal/Death。公开AdaptAbility/Status也没有实际游戏接线消费者，不能把函数存在算集成。

### F14 / P2 项目内容激活只广播预载，没有原子完成/回滚

DivineBeastsPresentationClientSubsystem.cpp:189先向平台发布Catalog，205调用RequestLogicalPreload，215-217即加入Active并成功返回；249-282仅保存ID及Broadcast，整个Game/Plugins无OnLogicalPreloadRequested订阅者、无Data调用、无完成回执。bRequiredPreload不会阻止发布；即使资源缺失或没人处理事件也处于“已激活”。Partial failure不可能回滚、Deactivation也只是广播Cancel。与P14/P15要求完整预检成功后发布、失败撤销租约冲突。内容Registry是工程清单，不自动代替此运行合同。

### F15 / P2 项目默认VFX映射不符合当前Definition逻辑ID并没有对应资产

DivineBeastsPresentationProjectCatalog.cpp:23/38填写`Presentation.DBA.World.Interaction.Committed.Default`、`Presentation.DBA.Village.Guidance.Ready.Default`，缺少当前GamePlatformId规定的`@version`；WorldSubsystem.cpp:38以TryParse转换后才AcquireDefinition。这些ID没有真实Definition/内容包文件、DBAPresentationPack_Core也未登记。所以合法WorldInteraction/VillageFeedback能被Catalog解析及Provider记录Submitted，但VFX最终拒绝/缺定义。应把默认映射视为规划合同，交付对应合法Definition后才开放成功路径。

### F16 / P2 Common/Hero的服务器禁止装配未在项目启用层贯彻

Game/DivineBeastsArena.uproject:19-31全局启用Common/12Hero且无TargetAllowList；每个Hero.uplugin:14无目标限制依赖DBAClient。Server.Target仅Enable DBAServer/DBAArena/Village，没有Disable这些全局启用项；Custom/DedicatedServer/DefaultPakFileRules.ini:8-12排除DBAClient/UI/FrontEnd/VFX/Surface，但没有Common/Hero。说明文档“Server Target不启用英雄美术包”与描述层不符。源码可证明未排除声明，不能凭此声称实际已Cook/链接；应对同一HEAD求Server闭包及干净Cook/最终容器确认。

### F17 / P3 中文职责/API说明及内容文档未同步

PresentationClientSubsystem.h:38-65、Context/Catalog大部分公开字段，Moba Types.h:50-55及74-95、DBA ContentPack.h:26-35、VFX/SFX多公开字段均仅英文标识或一句类型注释，没有参数/单位/空值/所有权/失败/线程说明。Camera Build.cs和模块入口没有职责头。符合命名扫描不等于AGENTS中文人工说明条款已满足。Village README:9仍把Main/Training写后续，实际已交付；FrontEnd README:33保留旧启动说明；正式历史文档计数可能过时但不自动回写历史证据。应逐受影响文件补说明并做人工审核，不为一致性擅自改稳定身份。

## 必须补的真实运行证据

1. 同一HEAD的Editor/Client/Server构建和干净Cook/Stage，AssetRegistry父类/硬引用/Primary ID/Bundle数据；Server必须同时审Common/Hero/UI/VFX/Surface及最小权威动画。
2. Presentation精确/父语义、同名跨目录、Scope优先、资格、目录失败原子回滚和注销后的迟到加载，不仅当前3个类型测试。
3. SFX极短完成、255Active+128Pending、Owner销毁/并发抢占/虚拟化、取消纠正、双LocalPlayer；Queued终态；5v5 Audio Insights。
4. VFX真实Niagara/EffectType/Definition/评审地图、AutoRelease池反复自然完成/取消、Data多世界租约、Composite间接环/总子数/失败传播、关键预警缺资源回退及迟到瞬时事件时效。
5. Surface真实MPC及Material/Function链、项目世界消费者、Shader编译与不同材质复杂度/设备GPU成本；避免仅生成MPC认为完整材质方案交付。
6. UI通过正确Editor及Monolith读取/编译/保存/重载树、父类、焦点/导航/可访问性；Modal覆盖再返回、GC软预载、重入、世界切换、小窗口/移动/真实鼠标；密码瞬态与事件驱动源码由UI主组另审。
7. MOBA迟到GameState/Pawn/Combat、重生/旅行、英雄/技能Context映射、其他玩家表现、预测确认撤销，五种竞技人数高密度表现。
8. 79个内容资产引用、骨架、材质、动画/源图来源、碰撞/导航/出生安全点/权威PCG结果及多客户端Village真实准入。历史Manifest不能作为本轮运行通过。

本组未执行任何UE运行检查或性能捕获。主代理本轮静态门禁的结果应在总报告引用其真实日志，不能归为本子组执行；本报告新增项只是源码审查与磁盘清单。




## 表现专项源码位置索引

| 发现 | 当前源码位置 |
| --- | --- |
| F01 | [GamePlatformPresentationClientSubsystem.cpp:130](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/GamePlatformPresentationClientSubsystem.cpp:130) |
| F02 | [GamePlatformPresentationClientSubsystem.cpp:392](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/GamePlatformPresentationClientSubsystem.cpp:392) |
| F03 | [GamePlatformVFXCatalogRegistry.cpp:30](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Resolution/GamePlatformVFXCatalogRegistry.cpp:30) |
| F04 | [GamePlatformVFXCompositeDefinition.cpp:29](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Definitions/GamePlatformVFXCompositeDefinition.cpp:29) |
| F05 | [GamePlatformVFXDefinition.h:81](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Definitions/GamePlatformVFXDefinition.h:81) |
| F06 | [GamePlatformSFXWorldSubsystem.cpp:108](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp:108) |
| F07 | [GamePlatformSFXWorldSubsystem.cpp:400](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp:400) |
| F08 | [IGamePlatformSFXService.h:20](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Public/Interfaces/IGamePlatformSFXService.h:20) |
| F09 | [GamePlatformSurfaceSettings.cpp:7](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Settings/GamePlatformSurfaceSettings.cpp:7) |
| F10 | [GamePlatformSurfaceWorldSubsystem.cpp:158](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Subsystems/GamePlatformSurfaceWorldSubsystem.cpp:158) |
| F11 | [GamePlatformUIManagerSubsystem.cpp:765](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Manager/GamePlatformUIManagerSubsystem.cpp:765) |
| F12 | [MobaPresentationRequestBuilder.cpp:7](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationRuntime/Private/MobaPresentationRequestBuilder.cpp:7) |
| F13 | [MobaPresentationClientSubsystem.cpp:144](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/Private/MobaPresentationClientSubsystem.cpp:144) |
| F14 | [DivineBeastsPresentationClientSubsystem.cpp:189](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp:189) |
| F15 | [DivineBeastsPresentationProjectCatalog.cpp:23](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsPresentationProjectCatalog.cpp:23) |
| F16 | [DivineBeastsArena.uproject:20](E:/poject/feebooz/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject:20) |
| F17 | [GamePlatformPresentationClientSubsystem.h:38](E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Public/GamePlatformPresentationClientSubsystem.h:38) |

## 修复与动态验收的执行顺序建议

1. 先补真实P1链和作用域所有权：每World竞技适配器、Gate/ASC/GameplayActive、可信Roster准入、重连Pawn、Quest无损对账；对World和五模式建立真实批准配置及资源，保留缺条件拒绝路径。
2. 收紧资源与请求生命周期：Data历史有界回收并保持普通软资源统一入口及调用者所有权、PCG终态回收、Navigation重复ID、Equipment结束/重复槽、Loading和UI/玩家服务重入，准入HTTP关停/接收上限；覆盖失败、取消、退出与迟到回调。
3. 统一内容解析和激活事务：P13精确优先/显式父回退/完全同键失败，Composite依赖预检，预加载真实完成后发布、失败撤销自身租约；合法Default ID、基础回退和可诊断终态。
4. 补应用可用性和装配：Retry、输入Profile行所有权/保存终态/PIE隔离、Progression/LiveOps事件、Settings与玩家服务生产Provider；按真实需求完成5个壳的保留/实施，不擅自填空或删除身份。
5. 补双层依赖声明与目标/资产隔离，完整中文合同和同步目录/接口/配置/迁移文档；按实际构建入口验证4目标，服务器干净Cook/Stage审核最小权威资源与纯表现分离。
6. 在上述正确性和原生回归基础上运行UE/联机/资产用例；按批准硬件、真实人数、场景与资产捕获Insights、Audio/Niagara与内存趋势，之后再决定索引/缓存/批处理优化。没有测量依据不重建第二套预算或资产管理器。

此处是审查后的整改顺序，未执行新的源码修改。完整证据保留在专项JSON/Markdown及当前检查日志；保存路径属于Saved瞬态材料，不替代正式交付或人工/运行验收。


