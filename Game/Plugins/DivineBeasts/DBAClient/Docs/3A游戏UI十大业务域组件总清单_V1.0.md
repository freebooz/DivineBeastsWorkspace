# 《神兽联盟》3A游戏UI十大业务域组件总清单 V1.0

> 编制：2026-10-09；工作区：DivineBeastsWorkspace（神兽联盟工作空间）；文档类型：**研究提炼的目标组件体系与实际工程差异**。
> 数据来源：Blizzard、Square Enix、Bungie、Riot Games、VALORANT等官方界面/更新说明。参考的是功能组织与交互模式，不复制第三方视觉素材、品牌、图标或受版权保护的布局。
> **状态规则：**“已有C++/平台机制”仅表示发现相应底层代码或已有框架，“已有蓝图”表示本地目录已有对应二进制资源，均不等于完成后端、PIE或Cook验收；“待实现”仅列为设计目标。示例数据绝不作为业务权威事实。

## 0. 分类口径、统计与唯一归属

本清单保留 Core（核心框架）／Account（账号与登录）／World（世界与任务）／Character（角色与成长）／Inventory（背包与装备）／Combat（战斗与HUD）／Social（社交组队）／Arena（竞技对局）／System（系统设置）／LiveOps（运营服务）十个一级业务域。每个组件有唯一`componentId`（组件标识）、中文名称、功能定义、阶段优先级、依赖层与验证状态。跨域共用的是**同一平台视觉组件的不同配置实例**，不应在多个业务域复制代码或重复建Widget基类。

| 一级域（英文/中文） | 总数 | P0一期必须 | P1后续重点 | P2按需扩展 |
| --- | ---: | ---: | ---: | ---: |
| Core（核心框架） | 24 | 14 | 8 | 2 |
| Account（账号与登录） | 16 | 7 | 6 | 3 |
| World（世界探索与任务） | 28 | 8 | 15 | 5 |
| Character（角色与成长） | 20 | 6 | 6 | 8 |
| Inventory（背包、物品与装备） | 23 | 5 | 10 | 8 |
| Combat（战斗与HUD） | 56 | 28 | 26 | 2 |
| Social（社交、组队与公会） | 21 | 4 | 9 | 8 |
| Arena（竞技、匹配与对局） | 26 | 15 | 7 | 4 |
| System（系统设置与辅助功能） | 26 | 2 | 21 | 3 |
| LiveOps（运营服务与活动） | 23 | 0 | 7 | 16 |
| **总计** | **263** | **89** | **115** | **59** |

说明：这是**可选3A游戏 UI 功能参考库**，不是任何一款游戏的刚性验收标准；P0是针对《神兽联盟》当前开发流程建议的优先级，部分复杂项目可能将P1/P2前置，具体以正式需求审批为准。

## 1. Core（核心框架）

默认业务所有权：GamePlatformUI（平台UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `Core.RootLayout` | 根布局 | 本地玩家统一界面层与输入焦点 | P0 | GamePlatform | 已有蓝图 |
| `Core.LayerStack` | 界面层栈 | 页面、模态、加载、系统层顺序和返回规则 | P0 | GamePlatform | 平台机制 |
| `Core.HUDOverlay` | 全局HUD容器 | 组合常驻战斗和世界提示 | P0 | GamePlatform | 待实现 |
| `Core.ScreenTransition` | 页面过渡 | 进入、退出、取消与转场安全处理 | P0 | GamePlatform | 待实现 |
| `Core.ModalDialog` | 模态对话框 | 确认、危险操作与重要错误提示 | P0 | GamePlatform | 平台机制 |
| `Core.ConfirmationPrompt` | 二次确认 | 删除、退出和不可逆操作确认 | P0 | GamePlatform | 待实现 |
| `Core.Toast` | 短提示 | 轻量成功、失败与通知提示 | P0 | GamePlatform | 平台机制 |
| `Core.ScreenBanner` | 屏幕横幅 | 高重要性系统/战斗信息 | P1 | GamePlatform | 待实现 |
| `Core.FloatingText` | 浮动文字 | 数值、奖励、状态的高频反馈 | P1 | GamePlatform | 平台机制 |
| `Core.Tooltip` | 悬浮提示 | 技能、物品与术语释义 | P0 | GamePlatform | 已有C++ |
| `Core.ContextMenu` | 上下文菜单 | 对象关联操作和权限状态 | P1 | GamePlatform | 待实现 |
| `Core.RadialMenu` | 径向菜单 | 手柄、触屏快速交互 | P2 | GamePlatform | 待实现 |
| `Core.LoadingOverlay` | 加载蒙层 | 地图和资源切换期间的阻断界面 | P0 | GamePlatform | 平台机制 |
| `Core.LoadingProgress` | 加载进度 | 真实阶段、百分比或未知进度 | P0 | GamePlatform | 平台机制 |
| `Core.BusyIndicator` | 忙碌反馈 | 网络请求待确认与重复提交保护 | P0 | GamePlatform | 待实现 |
| `Core.ErrorReconnect` | 错误重连 | 失败原因、重试和退回安全入口 | P0 | DivineBeasts | 已有C++ |
| `Core.NetworkIndicator` | 网络质量 | 延迟、掉包、断线与服务器状态 | P1 | GamePlatform | 待实现 |
| `Core.InputGlyph` | 设备输入图标 | 键鼠、手柄、触屏映射提示 | P0 | GamePlatform | 已有机制 |
| `Core.FocusNavigation` | 焦点导航 | 手柄/键盘焦点轮转与读屏顺序 | P0 | GamePlatform | 已有机制 |
| `Core.NotificationCenter` | 通知中心 | 队列、合并、优先级和未读 | P1 | GamePlatform | 待实现 |
| `Core.UITutorialCoachmark` | 界面引导标注 | 首次操作的聚焦遮罩和提示 | P1 | GamePlatform | 待实现 |
| `Core.HUDLayoutEditor` | HUD布局编辑器 | 尺寸、锚点、透明度及预设保存 | P1 | GamePlatform | 待实现 |
| `Core.HUDProfile` | 布局预设 | 按设备或操作模式切换布局 | P1 | GamePlatform | 设计 |
| `Core.ScreenshotMode` | 纯净截图模式 | 临时隐藏非关键界面并恢复 | P2 | GamePlatform | 待实现 |

## 2. Account（账号与登录）

默认业务所有权：DivineBeastsUIClient（项目公共UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `Account.Boot` | 启动页面 | 本地资源初始化、版本和流程状态 | P0 | DivineBeasts | 已有C++ |
| `Account.Login` | 账号登录 | 凭据输入、后端认证与失败处理 | P0 | DivineBeasts | 已有蓝图 |
| `Account.LoginMethod` | 登录方式选择 | 账号、第三方与预留登录通道 | P2 | DivineBeasts | 待实现 |
| `Account.TermsConsent` | 协议与隐私确认 | 版本化用户协议和必要确认 | P1 | DivineBeasts | 待实现 |
| `Account.ServerSelection` | 服务器选择 | 可用服务器和区域状态 | P1 | DivineBeasts | 待实现 |
| `Account.ServerQueue` | 登录排队 | 真实排队位置与超时 | P1 | DivineBeasts | 待实现 |
| `Account.AccountProfile` | 账号基本信息 | 已认证账号公开显示数据 | P1 | DivineBeasts | 待实现 |
| `Account.AccountRecovery` | 账号恢复 | 申诉、找回和重设入口 | P2 | DivineBeasts | 待实现 |
| `Account.CharacterRoster` | 已有角色列表 | 当前账号下角色卡片与空态 | P0 | DivineBeasts | 已有蓝图复用 |
| `Account.CharacterCreate` | 角色创建 | 名称、原型预览和创建提交 | P0 | DivineBeasts | 已有蓝图 |
| `Account.CharacterSelect` | 角色选择 | 选择、预览和进入世界 | P0 | DivineBeasts | 已有蓝图 |
| `Account.CharacterDeleteConfirm` | 角色删除确认 | 安全提示和服务器二次确认 | P1 | DivineBeasts | 待实现 |
| `Account.CharacterRename` | 角色改名 | 规则提示与重命名确认 | P2 | DivineBeasts | 待实现 |
| `Account.AccountRestriction` | 账号受限提示 | 冻结、封禁、异地登录及解决引导 | P1 | DivineBeasts | 待实现 |
| `Account.SessionRecovery` | 会话恢复 | 断线重登与既有角色恢复入口 | P0 | DivineBeasts | 已有流程机制 |
| `Account.Logout` | 登出确认 | 退出与资源/敏感输入清理 | P0 | DivineBeasts | 待实现 |

## 3. World（世界探索与任务）

默认业务所有权：GamePlatformWorld/Quest → DivineBeastsUIClient（世界与项目UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `World.Minimap` | 小地图 | 地形、玩家朝向和可见标记 | P0 | GamePlatform | 已有C++ |
| `World.WorldMap` | 世界地图 | 区域、缩放与可用路线 | P1 | DivineBeasts | 待实现 |
| `World.RegionMap` | 区域地图 | 聚落、洞穴和副本分区 | P1 | DivineBeasts | 待实现 |
| `World.FogOfWar` | 地图迷雾 | 仅显示已发现或允许显示的区域 | P1 | DivineBeasts | 待实现 |
| `World.WorldCompass` | 世界指南针 | 方向与目标距离 | P1 | GamePlatform | 待实现 |
| `World.Waypoint` | 路径点 | 玩家标点和队伍共享目的地 | P1 | GamePlatform | 待实现 |
| `World.NavigationRoute` | 路线导航 | 从世界导航事实生成路线提示 | P1 | DivineBeasts | 待实现 |
| `World.MapPOI` | 兴趣点标记 | NPC、商店、采集点与传送点 | P1 | GamePlatform | 待实现 |
| `World.RegionBanner` | 区域进入提示 | 区域名称、等级和安全信息 | P1 | DivineBeasts | 待实现 |
| `World.WorldNameplate` | 世界名称板 | 角色/NPC姓名及团队标识 | P0 | GamePlatform | 平台机制 |
| `World.OverheadHealth` | 头顶血条 | 友方、敌方和目标生命可视化 | P0 | GamePlatform | 平台机制 |
| `World.QuestTracker` | 任务追踪 | 目标列表、简要进度与定位 | P0 | GamePlatform | 已有C++ |
| `World.QuestJournal` | 任务日志 | 已接取、已完成和追踪切换 | P1 | DivineBeasts | 已有C++基础 |
| `World.QuestDetail` | 任务详情 | 目标、奖励、条件和文本 | P1 | DivineBeasts | 待实现 |
| `World.QuestMarker` | 任务世界标识 | 允许可见的目标与交付地点 | P0 | GamePlatform | 平台机制 |
| `World.InteractionPrompt` | 交互提示 | 交谈、拾取、采集、进入 | P0 | GamePlatform | 已有C++ |
| `World.NPCDialogue` | NPC对话框 | 本地化台词、头像和退出 | P0 | DivineBeasts | 待实现 |
| `World.DialogueChoice` | 对话选项 | 剧情分支与安全交互命令 | P1 | DivineBeasts | 待实现 |
| `World.GatheringPanel` | 采集面板 | 采集条件、进度与结果 | P1 | DivineBeasts | 待实现 |
| `World.LootPrompt` | 拾取提示 | 掉落可见性、拾取资格和物品信息 | P0 | DivineBeasts | 待实现 |
| `World.TravelPortal` | 传送交互 | 目的地、费用、准入及加载 | P1 | DivineBeasts | 待实现 |
| `World.MountVehicle` | 坐骑载具界面 | 速度、耐力、下车及载具技能 | P2 | DivineBeasts | 待实现 |
| `World.WorldEvent` | 世界事件追踪 | 区域事件、参与人数和倒计时 | P2 | DivineBeasts | 待实现 |
| `World.WeatherTime` | 天气与日夜 | 环境信息与玩法相关天气警告 | P2 | GamePlatform | 待实现 |
| `World.PartyWorldMarker` | 队友世界标记 | 可见队友位置与集结点 | P1 | GamePlatform | 平台机制 |
| `World.WorldPing` | 玩家标点 | 位置、危险、前进和撤退标注 | P1 | GamePlatform | 平台机制 |
| `World.ObjectInspect` | 世界对象查看 | 物品、建筑与交互对象概况 | P2 | DivineBeasts | 待实现 |
| `World.ResourceNode` | 资源点界面 | 允许发现的采集资源和刷新信息 | P2 | DivineBeasts | 待实现 |

## 4. Character（角色与成长）

默认业务所有权：GamePlatformCharacter/Progression → DivineBeastsUIClient（角色UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `Character.CharacterSheet` | 角色属性面板 | 生命、攻击、防御、移速等基础与衍生属性 | P1 | DivineBeasts | 待实现 |
| `Character.CharacterOverview` | 角色概况 | 英雄肖像、等级、职业/定位 | P0 | DivineBeasts | 已有C++基础 |
| `Character.CharacterPortrait` | 角色头像 | 等级、身份和在线状态 | P0 | GamePlatform | 已有C++ |
| `Character.CharacterChoiceEntry` | 角色选择卡片 | 现有角色预览与选择按钮 | P0 | DivineBeasts | 已有蓝图 |
| `Character.HeroChoiceEntry` | 英雄资格卡片 | 可选英雄和预览触发 | P0 | DivineBeasts | 已有蓝图 |
| `Character.ExperienceBar` | 经验条 | 当前经验、升级门槛与增量 | P1 | GamePlatform | 待实现 |
| `Character.LevelUpToast` | 升级通知 | 属性增长与可用点数提示 | P1 | DivineBeasts | 待实现 |
| `Character.SkillTree` | 技能树 | 技能分支、锁定和解锁条件 | P1 | DivineBeasts | 待实现 |
| `Character.SkillDetail` | 技能详情 | 效果、消耗、冷却和预览 | P0 | DivineBeasts | 待实现 |
| `Character.TalentTree` | 天赋树 | 天赋点、分支和重置预览 | P2 | DivineBeasts | 待实现 |
| `Character.MasteryProgress` | 熟练度 | 英雄/武器成长等级和奖励 | P2 | DivineBeasts | 待实现 |
| `Character.StatComparison` | 属性比较 | 装备变更前后只读属性差异 | P1 | DivineBeasts | 待实现 |
| `Character.Loadout` | 配置预设 | 技能、装备、外观组合切换 | P2 | DivineBeasts | 待实现 |
| `Character.AppearancePreview` | 角色外观预览 | 三维旋转与姿态，前端本地表现 | P0 | DivineBeasts | 已有C++与蓝图 |
| `Character.Customization` | 角色外观定制 | 模型、材质、装扮和预览 | P1 | DivineBeasts | 待实现 |
| `Character.PetCompanion` | 宠物随从面板 | 已拥有宠物属性和召唤入口 | P2 | DivineBeasts | 待实现 |
| `Character.MountCollection` | 坐骑图鉴 | 坐骑持有、预览和快捷召唤 | P2 | DivineBeasts | 待实现 |
| `Character.TitleBadge` | 称号徽章 | 名片、称号与展示配置 | P2 | DivineBeasts | 待实现 |
| `Character.ProgressionMilestones` | 成长里程碑 | 阶段奖励及进度事件 | P2 | DivineBeasts | 待实现 |
| `Character.CharacterHistory` | 角色战绩档案 | 经过授权的历史数据 | P2 | DivineBeasts | 待实现 |

## 5. Inventory（背包、物品与装备）

默认业务所有权：GamePlatformInventory/Equipment → DivineBeastsUIClient（背包UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `Inventory.InventoryScreen` | 背包主界面 | 分页/分类的物品库存 | P0 | DivineBeasts | 已有C++ |
| `Inventory.InventoryGrid` | 背包网格 | 物品格子与容量 | P0 | GamePlatform | 已有C++ |
| `Inventory.ItemSlot` | 物品槽位 | 图标、品质、数量与状态 | P0 | GamePlatform | 已有C++ |
| `Inventory.EquipSlots` | 装备槽位 | 当前装备部位和装备物品 | P1 | DivineBeasts | 待实现 |
| `Inventory.ItemTooltip` | 物品提示 | 装备词条、等级需求及说明 | P0 | GamePlatform | 已有C++ |
| `Inventory.CompareTooltip` | 装备对比 | 当前和候选装备属性对照 | P1 | DivineBeasts | 待实现 |
| `Inventory.ItemRarity` | 品质边框 | 品质/稀有度样式 | P1 | GamePlatform | 待实现 |
| `Inventory.ItemDragDrop` | 拖拽与放置 | 拾取、拖动、回退和无效位置 | P1 | GamePlatform | 待实现 |
| `Inventory.ItemActionMenu` | 物品操作菜单 | 使用、装备、丢弃、分解 | P1 | DivineBeasts | 待实现 |
| `Inventory.ItemSplitStack` | 堆叠拆分 | 数量输入与业务校验 | P1 | DivineBeasts | 待实现 |
| `Inventory.InventorySearch` | 背包搜索 | 名称和标签过滤 | P1 | GamePlatform | 待实现 |
| `Inventory.InventorySort` | 排序筛选 | 分类、品质、类型的可配置排序 | P1 | GamePlatform | 待实现 |
| `Inventory.Warehouse` | 仓库存储 | 仓库分页、容量与转移 | P2 | DivineBeasts | 待实现 |
| `Inventory.CurrencyWallet` | 货币资产 | 余额、类型和历史入口，只读账本事实 | P1 | DivineBeasts | 待实现 |
| `Inventory.PickupNotification` | 拾取通知 | 物品名称、数量和品质 | P0 | GamePlatform | 平台机制 |
| `Inventory.LootRoll` | 团队拾取分配 | 需求、贪婪、放弃及计时 | P2 | DivineBeasts | 待实现 |
| `Inventory.ItemBinding` | 绑定限制提示 | 不可交易、不可丢弃等规则 | P1 | DivineBeasts | 待实现 |
| `Inventory.DurabilityRepair` | 耐久维修 | 耐久状态、预计费用和确认 | P2 | DivineBeasts | 待实现 |
| `Inventory.Crafting` | 制作合成 | 配方、材料、资格、队列和结果 | P2 | DivineBeasts | 待实现 |
| `Inventory.MerchantShop` | NPC商店 | 价格、分类与购买/出售确认 | P2 | DivineBeasts | 待实现 |
| `Inventory.TradeWindow` | 玩家交易 | 双边确认、防替换及服务端授权 | P2 | DivineBeasts | 待实现 |
| `Inventory.ItemDisassemble` | 物品分解 | 预览回收内容和危险确认 | P2 | DivineBeasts | 待实现 |
| `Inventory.ItemEnhance` | 物品强化 | 所需材料、成功/失败提示 | P2 | DivineBeasts | 待实现 |

## 6. Combat（战斗与HUD）

默认业务所有权：GamePlatformCombat/AbilitySystem/UI → DivineBeastsUIClient（战斗UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `Combat.CombatHUD` | 战斗主HUD | 玩家资源、技能、目标和反馈组合 | P0 | DivineBeasts | 已有C++基础 |
| `Combat.PlayerResourceBar` | 玩家资源条 | 生命、护盾及各类战斗资源 | P0 | GamePlatform | 已有C++ |
| `Combat.PlayerPortrait` | 玩家头像 | 当前角色、等级与状态 | P0 | GamePlatform | 已有C++ |
| `Combat.TargetFrame` | 当前目标框 | 敌我目标生命、状态与等级 | P0 | GamePlatform | 待实现 |
| `Combat.FocusTargetFrame` | 焦点目标框 | 保留关注目标的生命和状态 | P1 | GamePlatform | 待实现 |
| `Combat.TargetOfTarget` | 目标的目标 | 当前目标仇恨/攻击对象 | P1 | GamePlatform | 待实现 |
| `Combat.BossHealthBar` | 首领血条 | 大型首领生命、多段阶段 | P0 | GamePlatform | 待实现 |
| `Combat.StaggerBar` | 失衡/韧性条 | 首领或单位失衡进度 | P1 | GamePlatform | 待实现 |
| `Combat.ShieldBar` | 护盾条 | 护盾数值与刷新表现 | P0 | GamePlatform | 已有C++ |
| `Combat.SpecialResource` | 专用能量条 | 怒气、气势、职业资源等显示 | P0 | GamePlatform | 已有C++ |
| `Combat.AbilityBar` | 技能栏 | 主动技能的排列和触发态 | P0 | DivineBeasts | 已有C++ |
| `Combat.AbilitySlot` | 技能单格 | 图标、热键、禁用态、充能 | P0 | GamePlatform | 已有C++ |
| `Combat.AbilityCooldown` | 技能冷却遮罩 | 剩余时间、进度与可用状态 | P0 | GamePlatform | 已有C++基础 |
| `Combat.GlobalCooldown` | 公共冷却 | 全局技能使用间隔提示 | P1 | GamePlatform | 待实现 |
| `Combat.AbilityCharge` | 技能充能 | 充能次数、蓄力进度 | P1 | GamePlatform | 待实现 |
| `Combat.CastBar` | 施法条 | 施法名称、剩余时间与完成进度 | P0 | GamePlatform | 待实现 |
| `Combat.ChannelBar` | 引导条 | 分段引导、取消和终止 | P1 | GamePlatform | 待实现 |
| `Combat.InterruptAlert` | 打断提示 | 可打断标志、已打断和失败原因 | P1 | GamePlatform | 待实现 |
| `Combat.CooldownManager` | 关键冷却管理器 | 自有、队友或重要外部冷却摘要 | P1 | GamePlatform | 待实现 |
| `Combat.ProcAlert` | 触发提示 | 被动生效、连招窗口和触发效果 | P1 | DivineBeasts | 待实现 |
| `Combat.BuffIcon` | 增益图标 | 正面效果、来源、层数和剩余时间 | P0 | GamePlatform | 已有C++基础 |
| `Combat.DebuffIcon` | 减益图标 | 负面效果、可驱散标记与倒计时 | P0 | GamePlatform | 已有C++基础 |
| `Combat.BuffTray` | 增益托盘 | 玩家增益单独布局和排序 | P0 | GamePlatform | 已有C++基础 |
| `Combat.DebuffTray` | 减益托盘 | 玩家减益独立布局与危险优先级 | P0 | GamePlatform | 已有C++基础 |
| `Combat.StatusEffectTray` | 统一状态效果托盘 | Buff/Debuff/其他效果的展示容器 | P0 | GamePlatform | 已有C++ |
| `Combat.StatusEffectDetail` | 效果详情 | 来源、描述、剩余时间和类型 | P1 | GamePlatform | 待实现 |
| `Combat.DispelIndicator` | 驱散标识 | 可驱散类型与资格提示 | P1 | GamePlatform | 待实现 |
| `Combat.ImmunityIndicator` | 免疫提示 | 无敌、不可控、免伤等显示 | P0 | GamePlatform | 待实现 |
| `Combat.CrowdControlBadge` | 控制效果徽章 | 眩晕、沉默、禁锢、击飞、恐惧等 | P0 | GamePlatform | 待实现 |
| `Combat.CrowdControlTimer` | 控制剩余时间 | 高优先级失控/锁定时间 | P0 | GamePlatform | 待实现 |
| `Combat.PeriodicDamage` | 持续伤害标记 | 中毒、灼烧等伤害持续效果 | P1 | GamePlatform | 待实现 |
| `Combat.PeriodicHealing` | 持续治疗标记 | 持续治疗的来源与剩余时间 | P1 | GamePlatform | 待实现 |
| `Combat.EffectStacks` | 效果层数 | 可叠加效果层数与阈值警报 | P0 | GamePlatform | 已有C++基础 |
| `Combat.EffectExpiration` | 效果到期提醒 | 到期前可选提醒与优先级 | P1 | GamePlatform | 待实现 |
| `Combat.TargetDebuffTray` | 目标减益栏 | 来自自己或队友的目标状态 | P0 | GamePlatform | 待实现 |
| `Combat.PartyAuraSummary` | 队友效果摘要 | 可选重要正负效果过滤 | P1 | GamePlatform | 待实现 |
| `Combat.BossMechanicDebuff` | 首领机制减益 | 点名、爆发前置和必要倒计时 | P0 | GamePlatform | 待实现 |
| `Combat.ThreatMeter` | 仇恨指示 | 威胁排序、当前仇恨目标 | P1 | GamePlatform | 待实现 |
| `Combat.CombatText` | 战斗飘字 | 伤害、治疗、吸收、暴击和免疫 | P0 | GamePlatform | 平台机制 |
| `Combat.HitFeedback` | 命中反馈 | 命中确认与实际伤害区分 | P0 | GamePlatform | 平台机制 |
| `Combat.DamageDirection` | 受击方向 | 方向性受击和距离提示 | P1 | GamePlatform | 待实现 |
| `Combat.CombatLog` | 战斗日志 | 伤害、治疗、效果与时间记录 | P1 | GamePlatform | 待实现 |
| `Combat.Crosshair` | 准星 | 瞄准、交互与技能瞄准 | P1 | GamePlatform | 待实现 |
| `Combat.RangeIndicator` | 射程指示 | 距离不足和能否施放提示 | P1 | GamePlatform | 待实现 |
| `Combat.AreaTelegraph` | 危险区域预警 | 地面AOE及阶段安全区提示 | P0 | GamePlatform | 待实现 |
| `Combat.TargetMarker` | 战斗目标标记 | 集火、治疗、控制等团队标识 | P1 | GamePlatform | 平台机制 |
| `Combat.BossCastAlert` | 首领施法预警 | 关键施法即将完成的提示 | P0 | GamePlatform | 待实现 |
| `Combat.EncounterTimeline` | 战斗机制时间轴 | 首领阶段与关键事件预告 | P1 | GamePlatform | 待实现 |
| `Combat.BossPhaseIndicator` | 首领阶段指示 | 当前阶段、阶段切换与机制解锁 | P1 | GamePlatform | 待实现 |
| `Combat.LowHealthWarning` | 低生命危险提示 | 临界值警告与可访问性辅助 | P0 | GamePlatform | 待实现 |
| `Combat.DeathOverlay` | 阵亡遮罩 | 阵亡原因、复活条件和队友救援 | P0 | DivineBeasts | 待实现 |
| `Combat.ReviveProgress` | 复活进度 | 队友复活、读条和取消原因 | P1 | DivineBeasts | 待实现 |
| `Combat.CombatSummary` | 战斗结果摘要 | 结算前的统计投影，不判断权威输赢 | P1 | DivineBeasts | 待实现 |
| `Combat.BattleStance` | 战斗姿态 | 姿态、武器模式、战斗/非战斗切换 | P1 | DivineBeasts | 待实现 |
| `Combat.DamageRecap` | 死亡伤害回顾 | 致死来源和事件时间顺序 | P2 | DivineBeasts | 待实现 |
| `Combat.PersonalCooldownProfile` | 个人状态显示预设 | 根据角色类型保存展示配置 | P2 | GamePlatform | 待实现 |

## 7. Social（社交、组队与公会）

默认业务所有权：GamePlatformOnline/Party → DivineBeastsUIClient（社交UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `Social.ChatPanel` | 聊天窗口 | 世界、附近、队伍与私聊消息 | P1 | DivineBeasts | 待实现 |
| `Social.ChatChannels` | 聊天频道 | 频道切换、过滤与未读 | P1 | DivineBeasts | 待实现 |
| `Social.PrivateChat` | 私聊消息 | 会话与消息状态 | P1 | DivineBeasts | 待实现 |
| `Social.FriendList` | 好友列表 | 好友在线状态和搜索 | P1 | DivineBeasts | 已有C++投影 |
| `Social.FriendInvite` | 好友申请 | 申请处理与屏蔽策略 | P1 | DivineBeasts | 待实现 |
| `Social.PartyPanel` | 队伍面板 | 成员、队长、申请和状态 | P0 | DivineBeasts | 已有C++基础 |
| `Social.PartyRoster` | 队伍成员HUD | 头像、生命、护盾和死亡态 | P0 | GamePlatform | 已有C++ |
| `Social.PartyInvite` | 组队邀请 | 同意、拒绝、过期状态 | P0 | DivineBeasts | 待实现 |
| `Social.PartyReady` | 队伍准备 | 成员就绪、取消与等待 | P0 | DivineBeasts | 待实现 |
| `Social.PartyRole` | 队伍角色标识 | 主坦、治疗、输出、辅助 | P1 | GamePlatform | 待实现 |
| `Social.PartyVote` | 队伍投票 | 更换目标、离队和踢人投票 | P2 | DivineBeasts | 待实现 |
| `Social.PartyPing` | 队伍标记 | 危险、集合、进攻和撤退 | P1 | GamePlatform | 待实现 |
| `Social.GuildPanel` | 公会界面 | 公会成员、公告与职位 | P2 | DivineBeasts | 待实现 |
| `Social.GuildActivities` | 公会活动 | 活动报名与准备状态 | P2 | DivineBeasts | 待实现 |
| `Social.VoicePanel` | 语音组队 | 队友说话、静音与麦克风状态 | P2 | GamePlatform | 待实现 |
| `Social.PlayerCard` | 玩家名片 | 头像、等级、身份及公开资料 | P1 | DivineBeasts | 待实现 |
| `Social.PlayerInspect` | 查看玩家 | 角色公开装备和战绩入口 | P2 | DivineBeasts | 待实现 |
| `Social.BlockReport` | 屏蔽与举报 | 权限校验、确认与结果通知 | P1 | DivineBeasts | 待实现 |
| `Social.EmoteWheel` | 表情动作轮盘 | 动作选择和快捷交流 | P2 | GamePlatform | 待实现 |
| `Social.RaidFrames` | 团队/大型队伍界面 | 队伍分组、治疗状态和控制效果 | P2 | GamePlatform | 待实现 |
| `Social.FriendPresence` | 好友动态 | 上线、离线和状态订阅 | P2 | DivineBeasts | 待实现 |

## 8. Arena（竞技、匹配与对局）

默认业务所有权：GamePlatformArenaClient（MOBA竞技）→ DBAArena（竞技项目UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `Arena.MatchEntry` | 匹配入口 | 1v1至5v5可选模式与参数 | P0 | DivineBeasts | 已有C++基础 |
| `Arena.MatchQueue` | 匹配排队 | 队列耗时、取消和人数限制 | P0 | MobaCommon | 已有C++基础 |
| `Arena.MatchReadyCheck` | 匹配确认 | 成功后准备确认及超时倒计时 | P0 | MobaCommon | 已有C++基础 |
| `Arena.DraftPick` | 英雄选择 | 队伍英雄确认与战术布局 | P0 | MobaCommon | 已有C++基础 |
| `Arena.DraftBan` | 禁用英雄 | 可选规则模式的禁选流程 | P2 | MobaCommon | 待实现 |
| `Arena.TeamComposition` | 阵容概览 | 各队英雄、定位和准备态 | P0 | MobaCommon | 待实现 |
| `Arena.ArenaHUD` | 竞技常驻HUD | 比分、时间、资源和快捷信息 | P0 | DivineBeasts | 已有C++ |
| `Arena.MatchClock` | 比赛计时 | 开局、结束、加时和暂停 | P0 | MobaCommon | 已有C++基础 |
| `Arena.Countdown` | 赛前倒计时 | 准备、开局和重生倒数 | P0 | MobaCommon | 已有C++基础 |
| `Arena.TeamScore` | 比分面板 | 队伍积分、队伍标识与击杀 | P0 | MobaCommon | 已有C++基础 |
| `Arena.Scoreboard` | 记分板 | 击杀/阵亡/助攻/经济等授权统计 | P0 | MobaCommon | 已有C++基础 |
| `Arena.KillFeed` | 击败播报 | 击败、助攻、连杀及来源 | P1 | MobaCommon | 待实现 |
| `Arena.RespawnTimer` | 复活倒计时 | 队伍和个人复活条件 | P0 | MobaCommon | 待实现 |
| `Arena.AliveCount` | 存活人数 | 队伍存活/阵亡状态 | P0 | MobaCommon | 待实现 |
| `Arena.ObjectiveBar` | 据点/目标进度 | 占领、护送或摧毁状态 | P1 | MobaCommon | 待实现 |
| `Arena.ObjectiveCountdown` | 地图目标倒计时 | 资源重生、首领刷新提示 | P1 | MobaCommon | 待实现 |
| `Arena.TeamUltimateStatus` | 队友大招状态 | 允许展示的技能就绪摘要 | P1 | MobaCommon | 待实现 |
| `Arena.ArenaMinimap` | 竞技小地图 | 队伍可见位置与目标 | P0 | MobaCommon | 待实现 |
| `Arena.MatchAlert` | 比赛阶段提示 | 开局、击败、连胜和反击 | P1 | MobaCommon | 待实现 |
| `Arena.SurrenderVote` | 投降/终止投票 | 正式规则允许时发起和计票 | P1 | MobaCommon | 待实现 |
| `Arena.DisconnectStatus` | 玩家断连状态 | 重连计时及补位限制 | P0 | MobaCommon | 待实现 |
| `Arena.SpectatorHUD` | 观战HUD | 观察目标、摄像机切换和延迟 | P2 | MobaCommon | 待实现 |
| `Arena.ReplayControls` | 回放控制 | 时间轴、倍速及事件跳转 | P2 | MobaCommon | 待实现 |
| `Arena.MatchResult` | 赛后结算 | 胜负事实、奖励与战绩 | P0 | MobaCommon | 已有C++基础 |
| `Arena.RankChange` | 排位变化 | 等级/分值变化及保护规则 | P2 | MobaCommon | 待实现 |
| `Arena.MatchHistory` | 对局历史 | 时间、模式、结果和参战英雄 | P1 | MobaCommon | 待实现 |

## 9. System（系统设置与辅助功能）

默认业务所有权：GamePlatformSettings/Input/UI/SFX（平台）→ DivineBeastsUIClient（项目菜单）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `System.SystemMenu` | 系统菜单 | 继续、设置、帮助、返回与退出 | P0 | DivineBeasts | 已有C++ |
| `System.VideoSettings` | 图像设置 | 分辨率、显示模式、画质和帧率 | P1 | GamePlatform | 已有服务 |
| `System.AudioSettings` | 音频设置 | 主音量、音效、音乐和语音 | P1 | GamePlatform | 已有服务 |
| `System.ControlsSettings` | 操作设置 | 键位、手柄和触屏映射 | P1 | GamePlatform | 已有服务 |
| `System.InputRebind` | 按键重绑 | 冲突处理、重置与确认 | P1 | GamePlatform | 待实现 |
| `System.MouseSensitivity` | 鼠标与镜头灵敏度 | 镜头、鼠标移动和缩放手感 | P1 | GamePlatform | 待实现 |
| `System.GamepadOptions` | 手柄设置 | 震动、死区和布局 | P1 | GamePlatform | 待实现 |
| `System.TouchControls` | 移动触控布局 | 虚拟摇杆、手势和按钮位置 | P1 | GamePlatform | 待实现 |
| `System.LanguageSelection` | 语言选择 | 文本、配音与语言包 | P1 | GamePlatform | 待实现 |
| `System.AccessibilityMenu` | 无障碍菜单 | 视觉、听觉和操作辅助设置 | P0 | GamePlatform | 已有C++机制 |
| `System.ColorBlindMode` | 色觉适配 | 不仅依靠红绿，增加轮廓/图形 | P1 | GamePlatform | 待实现 |
| `System.TextScale` | 文字缩放 | 字体大小、行高和溢出策略 | P1 | GamePlatform | 待实现 |
| `System.ScreenReader` | 读屏语义 | 焦点说明和可访问标签 | P2 | GamePlatform | 待实现 |
| `System.Subtitles` | 字幕系统 | 剧情字幕、说话者与大小 | P1 | GamePlatform | 待实现 |
| `System.HighContrast` | 高对比度 | 警示与操作控件更易辨认 | P1 | GamePlatform | 待实现 |
| `System.ReduceMotion` | 降低动态效果 | 转场与屏幕闪烁削减 | P1 | GamePlatform | 待实现 |
| `System.HUDLayout` | HUD编辑器 | 拖拽、缩放、显隐和布局重置 | P1 | GamePlatform | 已有配置框架 |
| `System.HUDPresets` | HUD预设 | 按设备、角色与竞技模式保存 | P1 | GamePlatform | 待实现 |
| `System.HUDOpacity` | HUD透明度 | 非关键元素与快捷面板透明度 | P1 | GamePlatform | 待实现 |
| `System.CombatDisplaySettings` | 战斗显示设置 | 飘字/增减益/团队效果筛选 | P1 | GamePlatform | 待实现 |
| `System.NotificationSettings` | 通知设置 | 重要性、声音与横幅规则 | P1 | GamePlatform | 待实现 |
| `System.NetworkDiagnostics` | 网络诊断 | 延迟、抖动、丢包与断线 | P2 | GamePlatform | 待实现 |
| `System.PerformanceOverlay` | 性能监控 | FPS、帧时、内存简要信息 | P2 | GamePlatform | 待实现 |
| `System.PrivacySettings` | 隐私设置 | 玩家可见信息、语音与社交范围 | P1 | DivineBeasts | 待实现 |
| `System.CustomerSupport` | 帮助与客服 | 问题反馈和FAQ | P1 | DivineBeasts | 待实现 |
| `System.ResetDefaults` | 恢复默认 | 逐域确认、撤销和持久化 | P1 | GamePlatform | 待实现 |

## 10. LiveOps（运营服务与活动）

默认业务所有权：GamePlatformLiveOps/CommerceUI → DivineBeastsUIClient（运营UI）。底层视觉原子可以跨域复用，数据访问必须从所属业务领域和授权视图模型流入。

| 标识（英文ID） | UI组件（中文） | 主要职责（中文） | 阶段 | 三层归属 | 目前证据级别 |
| --- | --- | --- | --- | --- | --- |
| `LiveOps.Announcements` | 公告中心 | 游戏维护、版本更新与安全通知 | P1 | DivineBeasts | 已有C++投影 |
| `LiveOps.Mail` | 游戏邮件 | 收件、标记已读和有效期 | P1 | DivineBeasts | 已有C++投影 |
| `LiveOps.MailRewards` | 邮件附件领取 | 可领取物品、授权与幂等反馈 | P1 | DivineBeasts | 待实现 |
| `LiveOps.EventHub` | 活动中心 | 当前活动列表与分类 | P1 | DivineBeasts | 已有C++投影 |
| `LiveOps.EventCalendar` | 活动日历 | 时间、报名、截止与入口 | P2 | DivineBeasts | 待实现 |
| `LiveOps.DailyMissions` | 每日目标 | 领取条件、任务进度与奖励 | P1 | DivineBeasts | 待实现 |
| `LiveOps.WeeklyMissions` | 每周任务 | 周期重置、进度与奖励 | P2 | DivineBeasts | 待实现 |
| `LiveOps.Achievements` | 成就面板 | 完成条件、称号和奖励查看 | P2 | DivineBeasts | 待实现 |
| `LiveOps.DailyLogin` | 每日签到 | 签到日历和连续奖励 | P2 | DivineBeasts | 待实现 |
| `LiveOps.BattlePass` | 赛季通行证 | 级别、经验、奖励轨道 | P2 | DivineBeasts | 待实现 |
| `LiveOps.SeasonOverview` | 赛季概况 | 赛季时间、目标与奖励 | P2 | DivineBeasts | 待实现 |
| `LiveOps.Leaderboards` | 排行榜 | 规则、分段和权限可见名单 | P2 | DivineBeasts | 待实现 |
| `LiveOps.EventRewardPreview` | 活动奖励预览 | 奖励内容、概率披露/限制说明 | P2 | DivineBeasts | 待实现 |
| `LiveOps.RewardToast` | 奖励通知 | 已确认奖励领取、入账结果 | P1 | GamePlatform | 平台机制 |
| `LiveOps.Storefront` | 商城界面 | 商品目录、分类与详情 | P2 | GamePlatform | 已有商业UI基础 |
| `LiveOps.OfferDetail` | 商品详情 | 服务端展示价和期限 | P2 | GamePlatform | 已有商业UI基础 |
| `LiveOps.PurchaseConfirmation` | 购买确认 | 商品、金额、币种及失败处理 | P2 | GamePlatform | 已有商业UI基础 |
| `LiveOps.OrderStatus` | 订单状态 | 待验证、成功、失败、取消 | P2 | GamePlatform | 已有商业UI基础 |
| `LiveOps.EntitlementInventory` | 权益收藏 | 已持有皮肤、称号与道具 | P2 | DivineBeasts | 待实现 |
| `LiveOps.RedeemCode` | 兑换码入口 | 资格、速率限制与后端核销反馈 | P2 | DivineBeasts | 待实现 |
| `LiveOps.Compensation` | 补偿领取 | 系统补偿信息及领取结果 | P2 | DivineBeasts | 待实现 |
| `LiveOps.PatchNotes` | 更新说明 | 版本差异、重要修复和新功能 | P1 | DivineBeasts | 待实现 |
| `LiveOps.SurveyFeedback` | 玩家问卷 | 可选调查、提交反馈与隐私说明 | P2 | DivineBeasts | 待实现 |

## 11. 与现有GamePlatformUI（游戏平台UI）真实工程的对照

| 实际工程类与路径（英文名后中文说明） | 当前覆盖能力 | 本次新增设计项 |
| --- | --- | --- |
| UGamePlatformResourceBarWidget（通用资源条） | 生命/盾/能量进度显示 | 分段首领HP、失衡条、职业资源语义和可访问文本 |
| UGamePlatformPortraitWidget（通用肖像） | 玩家头像、英雄头像基本只读状态 | 目标/焦点/队友头像样式、异步资源及访问限制 |
| UGamePlatformSlotWidget（槽位）与UGamePlatformSlotBarWidget（槽位栏） | 图标、计数、归一化冷却进度与一组槽位 | GCD（公共冷却）、充能、可施放原因、组合技触发 |
| UGamePlatformMinimapWidget（通用小地图） | 地图软引用、归一化标记与玩家朝向 | 缩放手势、战争迷雾、世界坐标授权投影和路线 |
| UGamePlatformPartyRosterWidget（通用队伍面板） | 成员头像、血量比例和就绪状态 | 队友重要Buff/Debuff、职业/定位、复活和远端冷却摘要 |
| UGamePlatformCountdownWidget（通用倒计时） | 按来源版本接收剩余秒数 | 竞技阶段、首领机制、Buff关键到期的统一共享更新时间源 |
| UGamePlatformStatusEffectTrayWidget（状态效果图标托盘） | 图标、正负标识、层数和剩余时间 | Buff/Debuff分区、施放者、可驱散类型、优先级、溢出和控制状态 |
| UGamePlatformQuestTrackerWidget（任务追踪） | 任务目标摘要与进度 | 世界任务显示规则、地图联动和屏幕空间约束 |
| UGamePlatformInteractionPromptWidget（交互提示） | 当前交互语义与可用性显示 | 键鼠/手柄/移动端输入指示、提示消歧 |
| UGamePlatformSettingRowWidget（设置单项） | 名称、值、等待和错误态 | HUD编辑模式、状态效果过滤、色觉适配及配置持久化 |
| UGamePlatformFeedbackService（游戏平台反馈服务） | 有界反馈池、去重和合并 | 伤害类型过滤、数字优先级、密集战斗折叠 |
| UGamePlatformWorldUIService（世界投影服务） | 名称板/世界标记与集中投影 | 目标/队友头顶Buff、敌方可见性与遮挡规则 |

当前资产边界：DBAUIPack_Core（神兽联盟核心UI内容包）已有6个Widget Blueprint（控件蓝图）：根布局、登录、角色创建、角色选择、英雄选择条目和角色选择卡片；原有两张纹理另计。此前32份Monolith布局JSON为**制作规格**而非真实.uasset。最新UI页面目录含16个公共Surface（界面表面）和6个竞技Surface，共22个，其中3个已登记业务页面有真实蓝图，其余19个待生产；这不代表本清单其他参考组件均已注册为页面。

## 12. 三层归属与增量目录建议

```text
Game/Plugins/GamePlatform/Presentation/GamePlatformUI/         # 第一层：跨游戏通用UI组件与状态显示机制
  Source/GamePlatformUIClient/Public/Components/                # 现有ResourceBar/Portrait/Slot/Minimap/PartyRoster/Countdown/StatusEffectTray等基类
  Source/GamePlatformUIClient/Public/Definitions/               # 可选扩展：状态显示策略和主题定义契约
  Docs/AAA状态效果UI设计规范_V1.0.md                            # Buff/Debuff专项设计与优先级、数据事件边界
Game/Plugins/MobaCommon/GamePlatformArena/                       # 第二层：MOBA竞技显示能力
  Source/GamePlatformArenaClient/Public/                         # 竞技HUD、ArenaViewModel、比分/团队阶段视图
Game/Plugins/DivineBeasts/DBAClient/                              # 第三层：项目世界、战斗、账号、社交和系统页面
  Source/DivineBeastsUIClient/Public/                              # 项目域ViewModel、Screen/HUD/Panel，仅组合业务事实
  Docs/3A游戏UI十大业务域组件总清单_V1.0.md                      # 本目标组件清单
  Docs/3A游戏UI十大业务域组件台账_V1.json                       # 机器可读完整台账（仅描述，非资产）
Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/ # 第三层公共项目视觉
  Content/UI/                                                    # 真实Widget Blueprint资源，由Monolith制作
Game/Plugins/DivineBeasts/DBAArena/                              # 第三层可选竞技视觉与页面
  Content/UI/                                                    # 竞技HUD、英雄选取、记分板、赛后结算等
```

**防重复原则：** 世界头顶血条属于World（使用通用资源条），玩家主HUD血条属于Combat（复用同一资源条），队伍成员血条属于Social或Arena（复用同一资源条）。Buff/Debuff统一在Combat拥有原始UI契约，World/Social/Arena只取不同可见性筛选后的视图。Settings（系统设置）拥有展示与个性化策略，不拥有GameplayEffect（游戏状态效果）的权威数据。

## 13. 数据更新、分期与质量门禁

1. 事件链：GAS/战斗/任务/在线/竞技等事实 → 授权客户端复制与领域Adapter（适配器） → ViewModel（只读投影） → Typed Delegate（类型委托） → Widget事件/增量渲染。Buff添加/叠层/刷新/移除是事件；剩余时间显示从共享时间基准更新，禁用逐Widget业务Tick。
2. 安全链：敌方状态、隐身、迷雾、道具、未解锁技能、账号资料、价格和奖励等均以服务端许可的展示数据为准，不得从客户端对象遍历推断隐藏信息；世界切换和断线恢复须校验SourceScope（来源作用域）与Generation（代次）。
3. 性能链：软引用图标、可见列表虚拟化/池化、增量排序与节流；战斗紧急告警必须有纯文字/形状基础回退，视觉皮肤失败不得影响规则。频率、最大个数和像素占比仅在实测/用户验收后定为正式预算。
4. 核验链：真正的Widget Blueprint（控件蓝图）须Monolith在锁定UE5.8编辑器中生成、编译、保存并回读；UHT/C++编译、Automation测试、PIE（编辑器内运行）、Windows客户端Cook（资源烘焙）、联机与移动真机均独立记状态。现阶段不改变或覆盖原有.uasset。

### 13.1 面向单人开发与现有项目流程的实际落地批次

- **M0（最小流程验证）**：仅选取现有RootLayout（根布局）、登录、角色创建/选择的真实蓝图，加上Boot（启动）、Loading（加载）、ErrorReconnect（重连错误）、World HUD（世界抬头显示）、QuestTracker（任务追踪）、InteractionPrompt（交互提示）等核心组合完成进入Village（新手村）的可观察链路。已有内容保留、不重复生产。
- **M1（战斗核心）**：PlayerResourceBar（玩家资源条）、TargetFrame（目标框）、AbilityBar（技能栏）、CastBar（施法条）、StatusEffectTray（状态效果）、BuffTray/DebuffTray（增益/减益分栏）、CrowdControlBadge（控制效果）、CombatText（战斗飘字）、BossCastAlert（首领施法预警）和低血量状态。此批必须先完成Buff/Debuff实例身份、观察者可见性与统一视图模型的设计审核。
- **M2（MOBA闭环）**：MatchQueue（匹配队列）、ReadyCheck（准备确认）、DraftPick（选人）、ArenaHUD（竞技HUD）、MatchClock（比赛计时）、Scoreboard（记分板）、RespawnTimer（复活计时）、KillFeed（击败播报）及MatchResult（赛后结算）。五种竞技规模使用一套通用竞技类与模式配置。
- **M3（丰富体验）**：完整背包装备比较、世界地图、社交聊天、好友/公会、HUD编辑模式、无障碍、运营活动和商城等。除新手必需页面外，按实际接口已就绪情况分批接入，避免先堆不真实的空面板。
- **每批统一验收**：目录登记/源码类 → Monolith真实Widget树和样式 → 编译保存回读 → ViewModel事件接线 → PC手柄和移动适配 → PIE与客户端Cook → 双客户端/服务器准入验证。未经验证保持“待交付”，不以目标库条目数替代真实完成率。

建议始终维护`Tests/Architecture/ValidateAAAUIInventory.py`（十大域研究台账一致性校验）与现有`Tests/Architecture/TestUIDeliveryInventory.ps1`（真实页面/蓝图交付校验）两套分离门禁：前者核对设计完整性，后者核对已登记与实际资产，两者通过不能自动推导联机功能通过。

## 14. 公开参考资料（功能依据，非项目已实现证明）

- 暴雪《魔兽世界》：可调整HUD与Debuff编辑器（2022年）：https://worldofwarcraft.blizzard.com/en-us/news/23837944
- 暴雪《魔兽世界》：冷却管理、个人资源、重要战斗预警：https://worldofwarcraft.blizzard.com/en-us/news/24223311
- 史克威尔艾尼克斯《最终幻想XIV》：战斗界面与增益/减益：https://na.finalfantasyxiv.com/uiguide/battle/
- 史克威尔艾尼克斯《最终幻想XIV》：HUD独立元素清单：https://na.finalfantasyxiv.com/lodestone/playguide/db/text_command/be2b72ee3f6/
- Bungie《命运2》：状态效果HUD优先级与可访问性（2024年）：https://www.bungie.net/7/en/News/Article/twid-04-11-2024
- Riot《英雄联盟手游》：可配置HUD与队友技能冷却（2024年）：https://wildrift.leagueoflegends.com/en-gb/news/game-updates/wild-rift-patch-notes-5-0/
- 暴雪《暗黑破坏神IV》：战斗飘字、地图标记与可读性设置：https://news.blizzard.com/en-us/article/24123440/diablo-iv-1-5-0-patch-notes
- Riot《无畏契约》：状态效果左右分区与击败播报（2026年）：https://playvalorant.com/en-gb/news/game-updates/valorant-patch-notes-12-05/
