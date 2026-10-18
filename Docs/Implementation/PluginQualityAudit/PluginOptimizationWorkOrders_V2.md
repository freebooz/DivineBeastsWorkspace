# SSLM 插件优化设计 V2.0：67个插件实施工作单

> 本文由当前真实67插件质量台账自动派生，按照M1—M6分组；每个条目都是未来待执行任务，不是已实施或人工审核通过。M0对所有插件执行基线检查，M7负责统一生产门禁。

- 当前工作单：67；机制插件：48；内容包：19；需要人工可视化：53；尚缺正式操作案例：51；人工通过：0。
- 已有诊断地图快捷入口涉及19个插件；诊断地图不作为正式可操作功能的完成证据。

## 各批次实施顺序

| 批次 | 中文职责 | 工作单数 | 执行依据 |
|---|---|---:|---|
| M1 | 运行期生命周期与安全 | 11 | 优先本批明确风险；前置M0、最终M7 |
| M2 | 客户端/专服构建与三世界 | 9 | 优先本批明确风险；前置M0、最终M7 |
| M3 | 唯一演示中心与人工审核 | 8 | 优先本批明确风险；前置M0、最终M7 |
| M4 | 视觉表现与环境 | 7 | 优先本批明确风险；前置M0、最终M7 |
| M5 | 权威玩法与场景 | 13 | 优先本批明确风险；前置M0、最终M7 |
| M6 | 19个内容包逐项交付 | 19 | 优先本批明确风险；前置M0、最终M7 |

## 每插件独立工作单

| 工作单 | 阶段 | 优先级 | 真实插件 | 核心优化与补全目标 | 人工操作入口 | 正式案例数 | 状态 |
|---|---|---|---|---|---|---:|---|
| SSLM-PLG-GamePlatformApplicationFlow | M1 | P2 | GamePlatformApplicationFlow | 复核登录到三世界的状态切换、失败回退和重复触发 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformCore | M1 | P2 | GamePlatformCore | 验证稳定身份、错误码、依赖不变量与失败结果 | 按职责采用可操作诊断面板/原生测试 | 3 | 未实施/未签收 |
| SSLM-PLG-GamePlatformData | M1 | P2 | GamePlatformData | 复核单一资产身份、并发加载合并、Owner租约和异步取消 | 按职责采用可操作诊断面板/原生测试 | 2 | 未实施/未签收 |
| SSLM-PLG-GamePlatformEntitlement | M1 | P2 | GamePlatformEntitlement | 验证权益来源、撤销、缓存隔离与权限失败收口 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformLiveOps | M1 | P2 | GamePlatformLiveOps | 验证运营配置版本、时间区间、过期行为和资格隔离 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformLoading | M1 | P1 | GamePlatformLoading | 验证资源就绪事件、超时/取消及UI加载态结束通知 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformOnline | M1 | P2 | GamePlatformOnline | 验证鉴权、连接时序、失败重试、过期响应与解绑 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformSave | M1 | P2 | GamePlatformSave | 验证权限、原子性、版本恢复与并发写入失败处理 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformServer | M1 | P2 | GamePlatformServer | 验证就绪、准入、排空、关服回调和Agones连接边界 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformSession | M1 | P2 | GamePlatformSession | 复验同步失败、取消、四事实Ready和ClientTravel旧代次阻断 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformTelemetry | M1 | P2 | GamePlatformTelemetry | 复验Ticker有限重试、关闭幂等、脱敏及长时待处理 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAArena | M2 | P1 | DBAArena | 验证竞技项目数据、出生/结算和真实服务器权威结果 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAServer | M2 | P2 | DBAServer | 验证项目准入、权威玩法配置和三角色服务器组合 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAWorlds | M2 | P1 | DBAWorlds | 验证Village与OpenWorld实际World定义、PCG及跨世界接入 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformArena | M2 | P1 | GamePlatformArena | 验证1v1—5v5模式数据、竞技实例和Client/Server职责 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformNavigation | M2 | P1 | GamePlatformNavigation | 验证服务器导航、动态障碍、路径失败和世界退出 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformPCG | M2 | P1 | GamePlatformPCG | 验证Gold图、实际Spawner输出、编排、烘焙与权威边界 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformWater | M2 | P1 | GamePlatformWater | 验证服务器水域权威信息与客户端材质/模拟资源隔离 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformWeather | M2 | P1 | GamePlatformWeather | 验证权威天气调度、客户端天气切换和失效快照 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformWorld | M2 | P1 | GamePlatformWorld | 验证三种服务器角色、世界代次和区域流送的订阅生命周期 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAClient | M3 | P2 | DBAClient | 完善唯一DemoHub、项目UI接线、异常反馈与真实操作入口 | 有诊断地图，仍待正式接线 | 2 | 未实施/未签收 |
| SSLM-PLG-GamePlatformCommerceUI | M3 | P1 | GamePlatformCommerceUI | 验证展示端与钱包/订单权威隔离、敏感信息清理 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformDebug | M3 | P1 | GamePlatformDebug | 验证开发模式开关、命令资格与Shipping隔离 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformDeveloperTools | M3 | P2 | GamePlatformDeveloperTools | 基于既有ReviewCase/Report建真实证据与实名人工签收入口 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformInput | M3 | P1 | GamePlatformInput | 验证键鼠/手柄/触屏输入映射、焦点冲突与失活回收 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformLocalization | M3 | P2 | GamePlatformLocalization | 核实真实文本与运行时切语言能力；模块壳不得冒充功能 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformSettings | M3 | P1 | GamePlatformSettings | 验证Provider参数应用、设备配置回退与权限隔离 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformUI | M3 | P2 | GamePlatformUI | 验证事件驱动层栈、焦点/输入、资源租约与页面重载 | 已有登记案例待运行 | 2 | 未实施/未签收 |
| SSLM-PLG-GamePlatformAnimation | M4 | P1 | GamePlatformAnimation | 验证动作蒙太奇/BlendSpace、根运动权威与动画事件取消 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformCamera | M4 | P1 | GamePlatformCamera | 验证游戏/角色/战斗镜头、混合、阻挡与复位 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformPresentation | M4 | P1 | GamePlatformPresentation | 验证中立请求、提供者注册、取消和目录回退 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformSFX | M4 | P1 | GamePlatformSFX | 验证播放/停止、空间化、资源缺失与音频实例回收 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformSurface | M4 | P1 | GamePlatformSurface | 验证雪、苔藓、湿润的共享材质函数与端侧隔离 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformVFX | M4 | P1 | GamePlatformVFX | 验证唯一Niagara服务、7类Effect Type、NDC降级与实例池 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-MobaPresentation | M4 | P1 | MobaPresentation | 验证竞技事件映射、不播放Niagara及卸载竞技后独立世界 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAGameplay | M5 | P1 | DBAGameplay | 验证生肖英雄数据/真实技能、GAS权限与Village出生流程 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformAbilitySystem | M5 | P2 | GamePlatformAbilitySystem | 复核GAS授权、冷却、费用、取消与服务器权威 | 按职责采用可操作诊断面板/原生测试 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformAI | M5 | P1 | GamePlatformAI | 验证感知、目标选择、导航和任务取消 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformCharacter | M5 | P1 | GamePlatformCharacter | 验证身份与运动组件、切角色、生命周期与复制 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformCombat | M5 | P1 | GamePlatformCombat | 验证权威伤害/治疗、命中重复、碰撞与表现解耦 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformEquipment | M5 | P1 | GamePlatformEquipment | 验证装备与背包事务、权限、撤销与角色同步 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformGameplay | M5 | P1 | GamePlatformGameplay | 验证通用玩法启动、停止和跨世界回收 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformInteraction | M5 | P1 | GamePlatformInteraction | 验证交互资格、距离、状态机和多人同时操作 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformInventory | M5 | P1 | GamePlatformInventory | 验证事务一致性、版本、幂等、撤销和资源隔离 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformLobby | M5 | P1 | GamePlatformLobby | 核实OpenWorld大厅真实消费者及模块壳缺口，不创建独立大厅服务器 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformProgression | M5 | P1 | GamePlatformProgression | 验证服务器进度事实、加点/升级与多次事件去重 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformQuest | M5 | P1 | GamePlatformQuest | 验证目标进度、奖励幂等和失败补偿 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-GamePlatformVillage | M5 | P1 | GamePlatformVillage | 核实Village教学/训练实际消费者及模块壳缺口 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAContentPack_Common | M6 | P1 | DBAContentPack_Common | 公共角色、模型/材质、共享资源与服务器权威依赖审核 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAFrontEndPack | M6 | P1 | DBAFrontEndPack | 登录、英雄选择与创建的真实Widget/场景及客户端权限 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Boar | M6 | P1 | DBAHeroPack_Boar | 审核亥猪真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Dog | M6 | P1 | DBAHeroPack_Dog | 审核戌狗真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Dragon | M6 | P1 | DBAHeroPack_Dragon | 审核辰龙真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Goat | M6 | P1 | DBAHeroPack_Goat | 审核未羊真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Horse | M6 | P1 | DBAHeroPack_Horse | 审核午马真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Monkey | M6 | P1 | DBAHeroPack_Monkey | 审核申猴真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Ox | M6 | P1 | DBAHeroPack_Ox | 审核丑牛真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Rabbit | M6 | P1 | DBAHeroPack_Rabbit | 审核卯兔真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Rat | M6 | P1 | DBAHeroPack_Rat | 审核子鼠真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Rooster | M6 | P1 | DBAHeroPack_Rooster | 审核酉鸡真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Snake | M6 | P1 | DBAHeroPack_Snake | 审核巳蛇真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAHeroPack_Tiger | M6 | P1 | DBAHeroPack_Tiger | 审核寅虎真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAPresentationPack_Core | M6 | P1 | DBAPresentationPack_Core | 项目VFX目录/定义与真实Niagara/材质消费者、性能和资源许可 | 有诊断地图，仍待正式接线 | 0 | 未实施/未签收 |
| SSLM-PLG-DBASFXPack_Core | M6 | P1 | DBASFXPack_Core | 项目音效资产、试听/停止、混音与服务器资源排除 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAUIPack_Core | M6 | P1 | DBAUIPack_Core | 项目页面、HUD、主题、事件交互、焦点与真实原生UI质量审核 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAWorldPack_MainArena | M6 | P1 | DBAWorldPack_MainArena | 主竞技场地图/掩体视野、五种竞技人数与静态环境性能 | 正式案例待制作 | 0 | 未实施/未签收 |
| SSLM-PLG-DBAWorldPack_Village | M6 | P1 | DBAWorldPack_Village | 新手村地图、森林农田/道路PCG、碰撞导航/出生与人工观察 | 正式案例待制作 | 0 | 未实施/未签收 |

## 所有工作单统一执行步骤

1. **01 功能与边界审查**：读取实际公开接口、已存在消费者、资产引用、端侧与已有测试，先确认功能分母。现有插件描述不能代替实际实现。
2. **02 最小代码/资源整改**：以实际复现的缺陷与测试为依据增量改代码；项目Widget Blueprint（控件蓝图）与布局必须使用Monolith，不能写文本假资产。
3. **03 人工界面与案例接入**：复用唯一DemoHub（演示中心）和真实开发地图；参数、执行、停止、重置和返回必须连接真实插件能力，禁止空按钮与自动人工通过。
4. **04 原生与发布证据**：按照当前UE5.8版本区分语法、UBT链接、UE Automation、Client/Server真实Cook、GPU/CPU实际测量，提供完整RunId与指纹。
5. **05 人工审核与交付**：由既有GamePlatformDeveloperTools ReviewSubsystem（人工审核子系统）生成待审核报告；真人逐证据确认，旧版本的通过不得投影为当前通过。

## 阻断与证据来源

- 优先级来自当前静态审查：P1多为缺可操作演示，并不代表对应插件有已确定代码故障。
- 编目及功能身份由真实源决定；本文件不在PluginDemoManifest（演示唯一作者源）写入未经确认的功能或案例。
- 不新增OpenWorld假地图，不恢复旧插件，不在当前活动UE编辑器中竞争写蓝图、关卡或表现资源。
- 本工作单状态固定为未实施；任何真实完成状态由实际代码/构建/运行/审核证据另行追加和验证。

详见：PluginQualityOptimizationDesign_V2_20261018.md（总体优化设计）及PluginQualityOptimizationExecutionPlan_V2_20261018.md（M0—M7执行计划）。
