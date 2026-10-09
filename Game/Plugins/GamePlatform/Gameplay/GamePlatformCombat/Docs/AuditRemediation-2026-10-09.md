# GamePlatformCombat：化身重置同步重入整改说明

本说明只覆盖2026-10-09新增的 `UGamePlatformCombatComponent::ResetForNewAvatar`（战斗化身重置）同步重入链和本组件护盾清理。平台组件拥有服务器权威战斗状态、GAS结算及临时护盾句柄；项目角色装配调用平台公开重置接口，客户端仅观察复制状态。调用、GAS通知及清理均在游戏线程，不产生后端请求、不加载项目视觉资产、不改权威伤害规则。

## 真实问题与引擎前提

锁定UE5.8中，`RemoveActiveEffectsWithGrantedTags`（移除带标签效果）和 `RemoveLooseGameplayTag`（移除松散标签）可在返回前调用原生Tag委托；`SetNumericAttributeBase`（设置属性基础值）同步发布属性值变化。生成的 `SetIncomingDamage`、`SetIncomingHealing`（设置伤害/治疗元属性）同样进入ASC，不能把它们当成无通知的普通赋值。

旧实现到这些外部调用全部结束后才设置 `bDead`（死亡状态）、`AvatarGeneration`（化身代次）、世界上下文代次并清空结果/需求。因此通知内已经完成的更高代次重置，或真实组件销毁触发的EndPlay，可被原调用返回栈覆盖。

## 操作身份、返回值与边界

- `ComponentLifecycleGeneration`（组件生命周期代次）只在BeginPlay建立和EndPlay关闭作用域时推进；`bCombatClosing`（正在关闭）在EndPlay任何GAS调用前置位，重复EndPlay幂等。下一次合法引擎BeginPlay才开放新作用域。
- `AvatarResetOperationGeneration`（化身重置操作代次）独立于复制的Avatar值，每次受理重置先登记。通知内后继重置会接管身份，旧调用不能在返回后继续写状态。
- 每次效果移除、标签移除、护盾移除、健康与两个元属性setter返回后，都复查生命周期、操作身份、当前Owner/World/ASC/AttributeSet、ASC真实绑定、关闭状态和化身/世界上下文代次。对象强引用只撑住当前同步栈，不授予关闭后的资格。
- 完整提交阶段没有外部通知：原子设置本次化身状态及清空本次旧结果。提交后校验的期望值跟随本操作自身合法推进，而操作身份保持独立，不能把正常重置误判成过期。
- `NewAvatarGeneration`不大于当前值时继续按既有接口自动推进。int32化身或世界上下文代次耗尽时在任何GAS操作前拒绝，避免回绕成旧身份。`HealthFraction`单位为比例，有限值限制0～1，非有限值继续按1处理。
- 返回true表示本操作完整提交且结束通知返回后仍属当前作用域；被后继重置或关闭接管返回false。已经完成的GAS写入不假装回滚；旧栈只停止后续写入，不恢复旧健康、旧代次或旧需求。关闭期间新的伤害/治疗/控制/护盾命令通过公共校验拒绝，不能在清理通知中新增护盾。

## 护盾句柄拥有

清理保存本批旧句柄身份，但每次外部移除前只从成员账本摘下本句柄。剩余旧句柄仍有所有者，通知内后继Reset或EndPlay可接管清理。每个移除返回后复查原作用域；失效立即停止，不清后继新盾，不在返回栈Reset整份账本。EndPlay使用受控关闭清理路径，可释放原作用域句柄，不开放新的请求资格。

## 真实回归源码与执行边界

本轮在现有 `Private/Tests/GamePlatformCombatAttributeTests.cpp` 添加八个简单自动化注册项，Flags均为 `EditorContext | EngineFilter`：

1. `GamePlatform.Combat.ResetReentry.NativeTagSuccessor`：真实Dead标签移除通知内高代次重置，新健康与治疗需求保留。
2. `GamePlatform.Combat.ResetReentry.NativeTagClosing`：真实标签通知内DestroyComponent路由EndPlay，旧栈及新重置停止。
3. `GamePlatform.Combat.ResetReentry.AttributeSuccessor`：真实健康变化通知内高代次接管。
4. `GamePlatform.Combat.ResetReentry.AttributeClosing`：健康通知内关闭，已完成属性写入保留，未提交化身代次不尾写。
5. `GamePlatform.Combat.ResetReentry.ShieldSuccessor`：多份真实护盾GE解除通知内重置并授新盾，仅后继新盾保留。
6. `GamePlatform.Combat.ResetReentry.NormalAdvance`：无重入时请求代次与自动推进都正常成功，防假门闩。
7. `GamePlatform.Combat.ResetReentry.ControlEffectSuccessor`：真实Stun GE解除通知覆盖第一处效果移除边界。
8. `GamePlatform.Combat.ResetReentry.MetaAttributeSuccessor`：生成的IncomingDamage setter通知内接管，后继IncomingHealing需求不被旧栈清空。

夹具只用于测试：真实无地图World、权威基础Actor、平台ASC与Combat组件；InitializeActorsForPlay后通过Actor的DispatchBeginPlay按引擎次序注册/开始组件。CreateWorld内部已有世界初始化，InitializationValues只作为第七参数传入一次。原生AddLambda委托在作用域结束先解绑，随后销毁Actor，再销毁同一个World一次；没有反射替身、跨帧引用、联网、GFrameCounter改动或伪造返回值。不需要真实地图、Widget或GI；这是ASC组件层回归，不代替项目角色、联机及跨图验证。

本轮按授权不运行UBT或UE实例。新增回归源码先于生产修复写入，但没有执行红灯/绿灯，不能声称已经复现运行失败或回归通过。静态辅助检查与源哈希、引擎证据、实际命令退出码统一记录于工作区Saved下 `MainIntegration/CombatResetRemediation.json`。中文人工复核限定本轮接口及调用路径，不代表全部历史Combat文件已满足全量中文规范。

## 兼容影响

保留插件、模块、公开反射类型、方法签名、协议、资产身份及原自动推进代次规则。新增私有生命周期/操作字段改变组件二进制布局，需重新编译插件及消费模块；不能与旧二进制混用。同步通知内被接管的调用改为明确false，调用方应以当前投影决定后续装配。没有新增生产Provider、角色类型或服务器表现依赖。
