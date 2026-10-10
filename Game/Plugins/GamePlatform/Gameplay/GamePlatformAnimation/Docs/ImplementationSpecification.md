# GamePlatformAnimation 实现规格

> 状态：P0-7设计规格；已增量实施客户端命中反馈与移动动画快照，其他规划能力未据此完成。
> 目标：建立跨游戏复用、服务器权威边界明确、项目内容可数据驱动扩展的 3A 动画基础插件。

## 1. 现状

当前保留 `GamePlatformAnimation（Runtime）` 与 `GamePlatformAnimationClient（ClientOnly）`。客户端已有本地顿帧、世界闪色，以及2026-10-10新增移动动画实例快照；完整权威动作合同、Definition、蒙太奇协调与全量Review仍按下述规格待实施，不能把局部能力称为完整动画系统。

插件现位于 `GamePlatform/Gameplay/GamePlatformAnimation`，描述 Category 已写为 Presentation。物理目录是否迁到 Presentation 必须单独做引用审计；P0 不为目录整齐迁移稳定插件身份。

## 2. 职责边界

Runtime 只拥有双端必须共享的动作语义和权威时序，不拥有纯客户端动画图。

应拥有：
- Animation Semantic（动画语义）与稳定 GameplayTag。
- Ability／Combat 可以引用的 Action Identity（动作身份）。
- Authority-required Montage／RootMotion 最小合同。
- Animation Action Snapshot（动作快照）、代次和取消语义。
- 服务器需要的 Root Motion／Montage Timing 规则。
- Definition 校验及 Data 租约身份。

ClientOnly 应拥有：
- Anim Layer／Linked Anim Graph 适配。
- Locomotion／Turn In Place／Aim Offset。
- Motion Warping。
- Foot Placement／IK。
- Montage Broker（蒙太奇协调）。
- Hit Reaction／Additive／Upper-Lower Body Layer。
- 纯表现动画事件到 Presentation 的连接。
- 本地质量和 LOD 策略。

禁止：
- 再建第二套 GAS 技能时序。
- 客户端 Animation 决定命中、伤害或权威位移。
- 服务器因少量 RootMotion 需求而被迫 Cook 全套 Mesh／客户端 AnimBP。
- 每个十二生肖建立独立 C++ Character/AnimSubsystem 派生树。

## 3. 推荐类型

### Runtime

```text
UGamePlatformAnimationActionDefinition
  : UGamePlatformDefinitionBase
```

建议字段：
- ActionId。
- RequiredGameplayTags／BlockedGameplayTags。
- AuthorityMode：PresentationOnly／AuthoritativeTiming／AuthoritativeRootMotion。
- Duration Policy／Section Identity。
- RootMotion Policy。
- RequiredDefinitions。
- Version／DataVersion 沿用 DefinitionBase。

建议公共接口：
- `IGamePlatformAnimationActionSource`：从 GAS／Combat 读取已确认动作事实。
- `FGamePlatformAnimationActionHandle`：含作用域、代次、激活身份。
- `FGamePlatformAnimationActionSnapshot`：只读动作事实。
- `IGamePlatformAnimationAuthorityPort`：仅权威时序需要时使用，不控制纯表现。

### ClientOnly

```text
UGamePlatformAnimationVisualProfileDefinition
  : UGamePlatformDefinitionBase
```

可通过软引用连接：
- Anim Instance／Linked Layer Class。
- Locomotion 配置。
- Montage／Sequence／Pose Search 或 Motion Matching 配置（项目真正采用时）。
- Motion Warping Target Schema。
- IK／Foot Placement Profile。
- Quality Variant。

运行实现优先 Character Component／AnimInstance Adapter，不创建三层 Subsystem 继承树。

## 4. 项目扩展

平台：
```text
UGamePlatformAnimationActionDefinition
```

项目如确有额外稳定结构：
```text
UDivineBeastsAnimationActionDefinition
    : UGamePlatformAnimationActionDefinition
```

但“子鼠攻击A、辰龙技能B”若只是资源和参数差异，应是 DataAsset 实例，不创建逐英雄 C++ 子类。

## 5. GAS／Combat 集成

- Ability 启动产生明确 ActionId／ActivationId。
- 服务端需要 RootMotion 或时序时，Runtime 合同参与权威验证。
- 客户端 Presentation 可以失败或降级，但不能改变 Ability／Combat 终态。
- Ability Cancel／Death／Stun 应按句柄和代次停止关联动画。
- 预测与确认必须可去重；不能预测播一次、服务器确认再播一次。

## 6. 端侧

Runtime：
- Client／Server／Editor 可见。
- 只允许 Server-safe 定义和必要时序。

Client：
- 仅 Client／Editor。
- AnimBP、Mesh、纯表现 Montage、IK、Motion Warping 视觉适配不进入 Server。

Editor：
- 后续按需要在本插件增加 Editor 模块；若验证能力可由 DeveloperTools + DataValidation 完成，不为形式新增空模块。

## 7. 测试与 Review

自动测试至少覆盖：
- Action 代次、取消、重复确认。
- Death／Respawn 后旧 Action 无效。
- Presentation 失败不改变权威状态。
- Server-only 构建不反向依赖 Client 模块。
- Definition 缺字段、版本和 RequiredDefinitions 失败。

人工 Review：
- Locomotion。
- Turn In Place。
- Foot IK。
- Motion Warping。
- Ability Montage。
- Cancel／Interrupt。
- Hit Reaction。
- 网络双客户端同步。
- LOD／质量降级。

真实 Review Map 由 Unreal Editor 生成；当前没有资产，不得标 G4 Passed。

## 8. 实施顺序

1. Definition／Types／接口和测试。
2. GAS／Combat Action 适配。
3. Client Profile 和 AnimInstance Adapter。
4. Locomotion／Montage／Motion Warping。
5. 项目 Profile 接入。
6. Review Profile／真实资产。
7. Editor／Client／Server 构建、Cook 和网络验证。

## 9. 2026-10-10 移动动画快照增量

`UGamePlatformLocomotionAnimInstance`位于客户端模块Public/Locomotion，供项目真实动画蓝图向下继承。引擎游戏线程动画评估读取当前Pawn的实际水平速度（厘米/秒）与CharacterMovement下落模式；本地角色和复制代理采用同一入口，不按输入意图伪造行走，也不读取账号、HTTP、UI或业务状态。初始化、无Pawn和动画失活都清除旧状态，不缓存所有者，不拥有委托、计时器或资源加载器。

项目动画资源拥有者决定Idle/Walk/Run/Fall资源与速度轴；平台类不引用项目挂载点、生肖或MOBA。纯表现采用原地动画，权威位移仍由角色移动组件计算。平台新增类不要求项目、MOBA再创建没有独立职责的中间C++类。

模块Private/Tests中的`GamePlatform.Animation.Locomotion.PawnMovement`使用真实瞬态Character与移动组件，覆盖水平速度、垂直速度排除、下落、停止、失活清理和无Pawn预览。该测试不修改正式地图或出生点；执行结果须以本轮锁定UE构建和自动化日志为准，源码存在不表示运行通过。项目资产检查与双客户端移动验收另外执行，不能用此用例替代Cook或联机视觉验收。
