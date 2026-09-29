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