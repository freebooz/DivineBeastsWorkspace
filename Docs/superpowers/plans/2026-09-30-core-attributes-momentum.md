# 《神兽联盟》核心属性与气势体系执行计划

版本：1.0
日期：2026-09-30
工作空间：DivineBeastsWorkspace（神兽联盟工作空间）

## 1. 目标

在不新增战斗/属性/气势插件、不恢复已退休旧五行玩法的前提下，把核心属性体系落入现有三层插件架构：

1. `GamePlatformAbilitySystem（游戏平台能力系统插件）`继续作为 GAS（Gameplay Ability System，玩法能力系统）基础设施和统一 `AttributeSet（属性集）` 类型边界。
2. `GamePlatformCombat（游戏平台战斗插件）`拥有跨游戏通用的生命/护盾、攻击、防御、控制/韧性和 Meta（临时结算）属性。
3. `DBAGameplay（神兽联盟玩法插件）`拥有项目专属 `Momentum（气势）` 属性与 Hero Definition（英雄定义）配置。
4. `DBAClient（神兽联盟客户端插件）`只订阅 Gameplay 属性变化并生成只读 UI 投影，不轮询、不拥有权威真值。
5. `Equipment（装备）`、`Progression（成长）`继续通过 GameplayEffect/AbilitySet 修改正式属性，不建立第二套数值系统。

## 2. 执行步骤

### P0｜保护工作树
- 保留现有 VFX 等未提交修改，不触碰无关文件。
- 增量修改现有插件，不重建工程、不新增同义插件。

### P1｜统一属性继承边界
- `UGamePlatformCombatAttributeSet（平台战斗属性集）`改为继承 `UGamePlatformAttributeSet（平台属性集基类）`。
- 所有新增平台战斗属性集同样继承该基类。

### P2｜完善平台战斗核心属性
- 保留 `UGamePlatformCombatAttributeSet` 作为 Vital + Meta（生命/护盾+结算元属性）主链，避免破坏现有 Damage/Healing Execution（伤害/治疗执行）。
- 新增 `UGamePlatformOffenseAttributeSet（平台攻击属性集）`。
- 新增 `UGamePlatformDefenseAttributeSet（平台防御属性集）`。
- 新增 `UGamePlatformControlAttributeSet（平台控制与韧性属性集）`。
- 所有可见属性使用 RepNotify（复制通知），并执行有限值/范围 Clamp（约束）。

### P3｜实现神兽联盟气势
- 在 `DivineBeastsCharactersRuntime（神兽联盟角色运行模块）`新增 `UDivineBeastsMomentumAttributeSet（神兽联盟气势属性集）`。
- 默认 `Momentum=0`、`MaxMomentum=100`、`MomentumGainMultiplier=1`、`MomentumDecayRate=0`。
- 新增 `FDivineBeastsMomentumDefinition（神兽联盟气势定义）` 并进入 `UDivineBeastsHeroDefinition（英雄定义）`。
- 服务器在已有平台 ASC 时确保气势属性集存在并按 Hero Definition 初始化；客户端只接收 GAS 复制。

### P4｜打通 UI 只读投影
- 新增 `UDivineBeastsPlayerStatusViewModel（玩家状态视图模型）`。
- 订阅 ASC 的 Health/Shield/Momentum 属性变化 Delegate（委托），无 Tick 轮询。
- 输出 `FDivineBeastsPlayerStatusViewData（玩家状态视图数据）` 给现有 `UDivineBeastsPlayerStatusPanel（玩家状态面板）`。

### P5｜测试与文档
- 增加平台属性默认值、继承边界与 Clamp 自动化测试。
- 增加气势定义/属性 Clamp 自动化测试。
- 更新 GamePlatformCombat、DBAGameplay、DBAClient 相关文档。
- 执行架构脚本、定向模块编译；不能完成的引擎运行验证必须明确列出。

## 3. 明确不做

- 不恢复 `Element（旧五行玩法）`、克制、破元、共鸣等已被现行工程规则禁止的历史机制。
- 不创建 `MobaCombat`、`MobaAttribute`、`DivineBeastsMomentum` 等新插件。
- 不把核心角色属性放入 `GamePlatformArena（竞技场插件）`，保证 OpenWorld/Village 不依赖竞技层。

## 4. 执行结果（2026-09-30）

- 已完成平台 AttributeSet 统一继承边界。
- 已完成攻击、防御、控制/韧性三个平台属性集；现有生命/护盾/Incoming Damage/Healing 主链保持兼容。
- 已完成 `FDivineBeastsMomentumDefinition`、`UDivineBeastsMomentumAttributeSet`、Hero Definition 配置与服务器初始化装配。
- 已完成 `UDivineBeastsPlayerStatusViewModel` 的 GAS 委托订阅及 PlayerStatusPanel 事件绑定，未增加业务 Tick。
- `ValidateProjectHeaders.ps1`：453 处自有头引用，0 缺失。
- UE5.8 `DivineBeastsArenaEditor Win64 Development` 定向构建 `GamePlatformCombat + DivineBeastsCharactersRuntime + DivineBeastsUIClient`：40 个动作，Result: Succeeded。
- 自动化测试启动未进入测试队列：本机 UnrealEditor 启动前的平台 SDK 校验因 VisionOS 缺失 `MainVersion` 失败；该结果属于环境阻断，不代表新增断言失败。
- `InheritanceBoundaryAudit.psm1` 直接执行被当前运行环境安全策略拦截，未取得该门禁运行证据；真实 UHT/C++ 编译已经验证新增跨层公开类型依赖可构建。
