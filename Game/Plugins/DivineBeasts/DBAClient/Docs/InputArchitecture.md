# DBAClient 神兽联盟用户输入架构

> 更新日期：2026-09-27。正式实现模块：DivineBeastsInputClient（神兽联盟输入客户端）。

## 1. 分层关系

```text
DivineBeastsInputClient
        ↓
GamePlatformInputClient
        ↓
GamePlatformData
        ↓
GamePlatformCore
```

项目层不复制 Enhanced Input、Context Lease、Block Lease、重绑定、Touch运行时或设备管理。

## 2. 项目输入语义

```text
DivineBeasts.Input.Combat.Primary
DivineBeasts.Input.Ability.Slot1
DivineBeasts.Input.Ability.Slot2
DivineBeasts.Input.Ability.Slot3
DivineBeasts.Input.Ability.Slot4
DivineBeasts.Input.Target.Lock
```

这些Tag属于DivineBeasts，不回灌GamePlatform。Move / Look / Menu / Confirm / Cancel 等通用能力直接使用 `EGamePlatformBuiltInInputSemantic（平台内建输入语义）`；项目运行时桥不再通过旧 `EGamePlatformInputSemantic` 取得平台通用语义。

## 3. 项目Profile

UDivineBeastsInputProfileDefinition 单向继承 UGamePlatformInputProfileDefinition，只增加项目完整性约束：Gameplay Profile必须包含主攻击、四技能槽和目标锁定；核心动作固定 Boolean + Actions Channel + Passthrough；禁止继续使用旧平台 AttackPrimary / AbilitySlot1～4 / TargetLock 枚举；纯PC/移动按键差异继续使用DataAsset实例。

## 4. Ability Input（能力输入）映射

攻击和技能槽映射到 `Platform.Ability.Input.DivineBeasts.*`；TargetLock不是技能激活，不映射为Ability Input。`UGamePlatformAbilitySystemComponent（平台能力系统组件）` 已正式实现 `IGamePlatformAbilityInputReceiver（平台能力输入接收器）`：按精确 InputTag 查找唯一技能Spec，使用 AvatarGeneration + ScopeId 拒绝旧Pawn输入，并支持 `OnPressed（按下激活）/WhileHeld（持续按住）` 两种显式策略。项目层只把 Semantic Tag 转换成 Ability Input Tag，不直接遍历或修改 GAS 私有状态。

## 5. 客户端事件桥

`UDivineBeastsInputClientSubsystem（神兽联盟输入客户端子系统）` 组合 `IGamePlatformInputService（游戏平台输入服务）`，不派生或复制平台Input子系统。它负责：异步准备项目Profile、通过平台状态事件申请Context并绑定当前EnhancedInputComponent；消费平台Move/Look语义驱动当前Pawn/Controller；把攻击/技能项目Tag送入平台ASC；把TargetLock转换为项目目标锁定请求事件；移动Touch仍调用平台 `BeginTouchInputBySemantic`。Controller/Pawn变化通过LocalPlayer生命周期和输入事件刷新，不创建业务Tick。
项目运行时桥有静态门禁：禁止重新使用平台旧 AttackPrimary / AbilitySlot1～4 / TargetLock 兼容枚举，也禁止通过旧枚举 API 读取 Move/Look；旧项目枚举只允许在 `UDivineBeastsInputProfileDefinition` 校验器中出现，用于明确拒绝尚未迁移的旧资产。

## 6. PC与移动端

PC键鼠、手柄和移动端最终全部进入同一 `GamePlatformInput CompactSlot（平台输入紧凑槽位）`，再由 `DivineBeastsInputClient` 进入同一 Move/Look/GAS/TargetLock 路由。移动UI只负责屏幕控件和手势结果，通过Touch句柄注入平台输入，不复制Gameplay逻辑；LookRate只在最终Controller消费端乘一次DeltaSeconds。

## 7. 当前边界

已实现：项目 Semantic Tag、项目 Profile 校验、项目输入事件桥、项目 Touch 语义入口、项目 Semantic→Ability Input Tag 映射、平台 Built-in Move/Look 消费、平台输入状态订阅、Profile/Context/Binding 自动装配、标准 Pawn 移动与 Controller 视角消费、平台 ASC 真实 Ability Input Receiver、目标锁定请求事件、项目默认输入 DeveloperSettings，以及 UE5.8 Editor/Win64 Client 定向模块编译。

性能基线：项目层无业务Tick；稳定服务按LocalPlayer缓存；Profile只通过GamePlatformData准备一次；Context/Binding均使用平台租约/句柄；状态订阅不启动常驻维护Ticker，只有真实快照变化才通知；Pawn/ASC只在Controller变化或输入事件发现Pawn变化时刷新。

尚未验收：正式 `InputAction / InputMappingContext / UDivineBeastsInputProfileDefinition` 资产、实际项目Pawn/PlayerState挂载并授权技能的完整链、目标选择系统订阅、Android/iOS真机设备桥、Client Cook/Stage。源码默认Profile身份保持为空，不伪造不存在的 `.uasset`。

2026-10-11 PC世界原生输入策略由DBAWorldsRuntime的`ADivineBeastsWorldPlayerController`承接，Controller优先消费既有Pawn原生轴，W/S前后、A/D转身、Q/E横移、RMB+A/D横移、双鼠标键前进、Space跳跃。普通世界采用显示光标的GameAndUI；只有从视口开始的LMB/RMB拖动临时捕获，最后释放恢复原坐标。镜头观察角度与角色ControlRotation分离，RMB才同步朝向；滚轮调整SpringArm距离并保留原生碰撞。前端预览独立Actor不改动。

项目默认Enhanced Profile仍未配置，当前PC路径不会同时激活第二套Move/Look消费。未来正式Profile启用时必须迁移PC移动/观察语义到同一世界策略，不能保留相同物理键的双重运动消费；Touch/Gamepad的正式映射仍需独立运行验收。UI客户端通过世界Controller的中立ESC委托与本地暂停接口协调焦点，菜单/失焦/旅行清空旧按住状态；不存在底层向UI或项目层的反向依赖。当前原生数字技能1—4与移动按键无冲突。

世界拖动资格另检查当次Slate命中路径：只有当前游戏实际SViewport空白可首次捕获，禁用控件及未处理鼠标键的按钮同样阻止捕获，已捕获后第二个鼠标键可组合前进，释放始终清理。World控制器只增减自身Move/Look锁，不清零其他系统的Ignore输入计数；共享服务器目标不链接Slate和UIClient。

2026-10-11 技能栏的快捷键文字由项目`UDivineBeastsAbilityBarPanel`在构造、换Pawn与技能快照事件中刷新。四个可激活槽分别查询现有`DBA.PrimaryAbility`、`DBA.AbilityOne`、`DBA.AbilityTwo`、`DBA.UltimateAbility`动作的当前`PlayerInput.GetKeysForAction`映射；暂无PlayerInput时只读当前UE InputSettings，未映射时留空。默认配置显示1—4，但显示代码不硬编码数字；实际修饰键与多个键均保留，被动槽不显示激活键。这里只更新已有`HotkeyText`，固定12号字且非命中，不创建控件、不改映射、不改变GAS可用资格，也不让平台认识项目动作。未来运行中重映射命令应在完成事件显式刷新技能栏，不能以Tick轮询。纯格式自动化覆盖空映射、普通键、修饰组合、多绑定去重、非PC键过滤与被动无键；测试不代替资产、Cook或人工显示验收。
