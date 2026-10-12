# 头像及开发火焰、寒冰、螺旋奥术技能实施计划

> 执行：使用 executing-plans 分项实现；头像测试准备可并行，编辑器资产串行操作。用户已要求直接执行、只在 main 修改，不新建分支、不提交或推送。计划及验收记录留在项目，不创建云端页面。

**目标：** 头像仅显示真实角色头脸，技能栏显示当前角色的实际装载技能；提供火焰弹、寒冰箭、螺旋奥术飞弹及对应VFX用于开发伤害测试，后续可按正式技能替换。

**架构：** 复用项目角色身份、开发AbilitySet、GAS成本/冷却和平台Combat伤害结算。通用权威投射物与直线/螺旋运动合同放既有GamePlatformCombat；DBAGameplay提供项目开发施法派生与数据，客户端从复制事实提交中立表现请求，由唯一平台VFX框架执行。非竞技测试不强制依赖MobaCommon，不机械创建三层同名类；HUD不授权技能、不直接播放Niagara。正式英雄DefaultAbilitySetId保持不变。

**工具链：** UE5.8、现有MSVC、Monolith MCP 0.23.0、现有PowerShell构建发布脚本。

**设计基线：** 本文件下节；来源为本会话2026-10-12用户关于头像、技能栏和临时火焰/寒冰魔法的明确指令。

## 设计基线及约束

- 只在 `E:/work/2026/DivineBeastsWorkspace` 的 main 原位增量修改，保留现有未提交内容及正在人工验证的两个客户端。
- 头像原稿512×512头肩图不改RGB、不重导入；项目头像画刷采用归一化UV `(0.25,0.08)..(0.75,0.58)`，保留完整头部和面部，排除胸肩；正式生肖外形仍待制作，不能把当前开发人形称为正式肖像。
- 技能栏按当前Pawn/Hero/AvatarGeneration的授权快照及真实ASC Spec更新。使用 `AbilityIcon` 等已有命名控件；空、迟到、缺图标、切角色和解绑均清除旧投影，不用固定图标假装已授权。
- 临时火焰弹占现有主动槽1（当前默认按键2），寒冰箭占主动槽2（当前默认按键3），奥术飞弹占开发集合终极槽（当前默认按键4）；保留普攻/被动，不改变输入映射及正式英雄配置。每个英雄仍有自己的技能定义身份，执行器与VFX共用。
- 以下为可替换测试参数而非正式平衡方案：火焰弹基础伤害40、气势消耗10、冷却2秒、射程2000厘米、速度1200厘米/秒；寒冰箭基础伤害30、消耗8、冷却1.5秒、射程2000厘米、速度1600厘米/秒。只实现单目标魔法伤害，不冒充已实现燃烧DOT、冰冻、减速或完整火法体系。
- 奥术飞弹一次Commit后发出3枚，每枚基础伤害12、间隔0.2秒，总消耗12、冷却3秒、轴向射程2000厘米、速度1400厘米/秒、螺旋半径18厘米、频率3转/秒。螺旋为弹体实际位置及服务器有界细分Sweep轨迹，不只是尾迹。取消、死亡、换化身或世界退出清除尚未释放的定时器；已发射弹体失效来源不能继续伤害。
- 开发集合仍须非Shipping/Test、`bAllowDevelopmentAbilitySets=true`和`-DBADevelopmentSkills`共同启用。只修改已存在的开发集合/客户端Profile；正式DefaultAbilitySetId及正式发行配置不变。
- 火焰VFX为橙金核心、连续火焰尾迹、命中短促热浪；寒冰VFX为蓝白冰晶箭、冷雾尾迹、命中冰晶碎散；奥术VFX为紫蓝能量核、螺旋运行带与命中能量涟漪。采用原创资源，不复制魔兽世界资产或图标。投射物VFX不是权威碰撞，特效缺失不改变伤害。
- 客户端纯图标/材质/Niagara/目录属于项目表现内容；服务器产物剥离这些资源。全部真实资产通过Monolith创建/修改、编译、保存、重载留证。新图标必须满足现行纯净温润视觉和真实sRGB/RHI门禁，不用Gamma错误截图路径。

## 重点回归

1. 切Pawn、切英雄及重连的迟到资源完成不能覆盖新角色头像或图标。
2. 授权快照先于GAS Spec复制时只显示待确认，不标记技能可用。
3. 无目标、遮挡、自己、无效目标、过期Source代次和重复碰撞不造成重复或越权伤害。
4. 世界退出、投射物到期和命中后清理运动、委托及自有VFX句柄；观察者不重复播放。
5. 默认/Shipping/Test不启用开发技能，Dedicated Server不Cook客户端表现资产。

## 任务1 真实头像裁图和技能控件投影

文件：现有 `DivineBeastsPlayerPortraitWidget.h/.cpp`、`DivineBeastsAbilityBarPanel.cpp`、`Private/Tests/DivineBeastsVillageHUDTests.cpp`及`DivineBeastsCombatUIFeedbackTests.cpp`；真实Widget仍在DBAUIPack_Core。

- [ ] 先补 `DivineBeasts.UI.Village.PortraitFaceCrop` 与技能图标实际画刷回归，完整Editor编译并观察RED。
- [ ] 项目头像画刷裁取头脸；技能Panel把真实SlotState图标/冷却写入已有控件，不改平台权威及资产所有权。
- [ ] Monolith复核真实命名控件、编译保存重载；原生测试GREEN及真实RHI检查。

## 任务2 开发投射物权威及客户端表现

文件：GamePlatformCombat新增 `Actors/GamePlatformCombatProjectile`、`Types/GamePlatformCombatProjectileMotion`及Private/Tests；DBAGameplay的DivineBeastsAbilitiesRuntime新增 `DivineBeastsDevelopmentMagicAbility`、`DivineBeastsDevelopmentMagicDefinition`及Private/Tests；DBAClient的DivineBeastsPresentationClient新增世界表现适配，复用平台Data及Presentation/VFX接口。

接口：Ability只在合法Commit后生成服务器投射物；投射物发布类型、唯一事件身份、出生/命中/结束与代次事实，不携带纯表现资产引用。客户端适配根据复制事实消费项目内容目录。

- [ ] 先写资产/类缺失及真实世界碰撞、一次结算、无权威/过期来源/结束清理回归，观察RED。
- [ ] 实现共用权威投射物，服务器沿真实直线/螺旋路径做有界细分球体Sweep；仅必要运动开启服务器Tick。伤害交给GamePlatformCombat，不另造公式。
- [ ] 增量接入中立表现，覆盖独立请求身份、取消及异步失败；原生正常/异常用例GREEN。

## 任务3 作者资源与开发装配

文件：`Tools/Unreal/Abilities/AuthorDevelopmentMagicTests.py`、对应资产门禁；两个开发CustomConfig及现有开发集合/Profile。公共视觉放DBAPresentationPack_Core，公共图标放DBAUIPack_Core，不新增插件。

- [ ] 先运行实际资产缺失与装配门禁，记录RED。
- [ ] 通过Monolith制作三种技能及飞行/命中VFX，为12英雄增量替换两个开发主动槽及开发终极槽，保存并独立重载。
- [ ] 检查身份、数值、成本/冷却、图标及VFX编译；旧开发候选及正式英雄资源不删除。

## 任务4 构建发布及人工入口

- [ ] 执行完整UI/技能/表现原生回归及Tests/Architecture，逐项报告失败，不能只跑新增用例。
- [ ] 实际Editor/Client/Server构建、开发Client/Server干净Cook与容器/颜色/端侧门禁。
- [ ] 运行明确隔离的开发新手村专服及两个新版空白登录客户端，不自动提交账号密码，不中断现有验证进程。
- [ ] 记录实际进程、登录首屏、伤害与两端VFX检查；人工网络施法/视觉未确认前不记完成。

## 执行记录

初始main为226d4802；既有未提交文档及FileOpenOrder生成日志保留。Monolith0.23.0已重新启动并核对正式uproject、脏包0。实际AbilitySlot蓝图EventGraph只有未连接的PreConstruct/Construct/Tick，无SlotState到AbilityIcon的显示接线；普通发布配置未启用开发技能。源头像目视确认仍含胸肩，是本轮裁图输入。
