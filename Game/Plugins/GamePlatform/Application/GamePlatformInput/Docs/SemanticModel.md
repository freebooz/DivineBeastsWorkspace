# GamePlatformInput Semantic（输入语义）模型

> 更新日期：2026-09-27。本文定义跨游戏可扩展输入语义、Profile编译和高频运行时边界。

## 1. 目标

GamePlatformInput（游戏平台输入）只提供输入机制，不固定某个游戏“有几个技能”。

```text
FGameplayTag / FGamePlatformInputSemanticId
        ↓
FGamePlatformInputSemanticDescriptor
        ↓
UGamePlatformInputProfileDefinition
        ↓
InputProfileCompiler（一次编译）
        ↓
CompactSlot（紧凑槽位）
        ↓
Enhanced Input高频事件 O(1) 数组访问
```

编辑期允许平台、MOBA或项目层使用不同Tag；运行期不以GameplayTag做高频路由。

## 2. 平台长期语义

```text
Platform.Input.Move
Platform.Input.LookDelta
Platform.Input.LookRate
Platform.Input.Interact
Platform.Input.Menu
Platform.Input.Confirm
Platform.Input.Cancel
```

旧 EGamePlatformInputSemantic 中的 AttackPrimary、AbilitySlot1～4、TargetLock 仅保留兼容，不再作为新项目扩展入口。旧标签字符串保持不变，避免已有资产/API兼容破坏。

新代码访问平台公共语义统一使用 `EGamePlatformBuiltInInputSemantic（平台内建输入语义）` + `GetBuiltInSemanticTag/GetBuiltInSemanticDescriptor`。该枚举只包含 Move、LookDelta、LookRate、Interact、Menu、Confirm、Cancel；不会再加入攻击、技能槽或项目玩法动作。旧 `EGamePlatformInputSemantic` 保留完整旧值，仅作为资产/API迁移桥。

## 3. Descriptor（语义描述）

FGamePlatformInputSemanticDescriptor 包含 SemanticId、Unit、ValueType、ChannelMask、ValuePolicy。平台只验证数值合同，不限制项目Tag命名空间。

## 4. CompactSlot（紧凑槽位）

Profile准备完成后，CompileGamePlatformInputProfile 一次生成 CompiledActions[Slot]、SemanticTag→Slot 低频索引、LegacyEnum→Slot 兼容索引和等长 ActionGate 数组。Enhanced Input绑定时直接捕获 Slot，高频 Route / Interrupt 不执行Tag查询、TMap查找或运行期Descriptor解析。

## 5. Channel（输入通道）

Semantic可扩展，Channel保持固定bit mask：Move、Look、Actions、UICommands、TextEntry。BlockLedger在低频租约变更时维护缓存掩码，高频判断只进行位运算。

## 6. 旧枚举兼容

旧API继续支持 BeginTouchInput(PointerId, EGamePlatformInputSemantic, ...)，并通过 GetLegacySemanticDescriptor 转成统一Descriptor。新项目代码使用 FGamePlatformInputSemanticId + BeginTouchInputBySemantic；不得继续往旧平台枚举增加项目技能。

## 7. 三层扩展

```text
GamePlatformInput → 平台机制 + 通用Semantic合同
MobaCommon       → 仅真实多项目复用时的MOBA公共Semantic
DivineBeasts     → DivineBeasts.Input.* 项目Semantic
```

纯按键、InputAction、MappingContext、灵敏度和设备差异优先使用DataAsset实例，不为内容差异创建C++子类。

《神兽联盟》当前直接定义 `DivineBeasts.Input.*` 项目语义，不机械建立 MobaCommon 中间语义层；只有第二个以上 MOBA 项目证明同一语义合同需要复用时，才把相应合同上移到 MobaCommon。

## 8. 性能边界

- Profile编译只发生在Data租约准备完成后。
- 高频Enhanced Input回调只使用Slot数组。
- Touch项目语义Tag查询仅发生在BeginTouchInputBySemantic低频入口。
- 不在鼠标/摇杆样本路径分配Descriptor、查GameplayTag容器或加载资源。
- 旧兼容枚举不会降低新版项目语义运行时性能。