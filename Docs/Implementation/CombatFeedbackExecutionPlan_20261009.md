# 战斗打击反馈系统：三层插件实施计划与验收台账

日期：2026-10-09；工程：DivineBeastsWorkspace / Game/DivineBeastsArena.uproject（UE5.8）；任务范围：平台通用打击反馈、MOBA强度策略、神兽联盟英雄技能配置。

## 一、设计约束与当前基线（P0）

- 强制遵守 AGENTS.md v1.4.3：DivineBeasts（项目层）→ MobaCommon（MOBA层）→ GamePlatform（平台层），禁止平台/MOBA反向依赖；不创建新插件、不恢复废止的五行、破元和共鸣玩法。
- 当前46个机制插件＋已登记内容包保持不变；无真实引擎资产时不能伪造 .uasset。
- 检查确认 `GamePlatformCombat` 已有权威 `FGamePlatformCombatEvent`（GUID、角色代次、位置、结果标签、伤害），及 GameplayCue。注意 `OnCombatEvent` 普通委托不会自动在服务器和客户端间传输。
- `GamePlatformPresentation` 已有跨UI/VFX/SFX平台中立请求、Provider目录、LocalPlayer服务和世界代次；`MobaPresentation` 已有事实适配与GUID去重；现有 VFX/SFX Provider 必须复用。
- 开始前工作树已有别的任务针对 GAS 属性/战斗组件的未提交修改。本任务不得覆盖、不清除这些改动。

## 二、插件实现分配

### 第一层 GamePlatform：通用机制与客户端表现

1. `GamePlatformCombat`：保留唯一权威伤害、护盾、控制、命中验证和GameplayCue通道。禁止将局部视觉停顿引入权威伤害计算。
2. `GamePlatformPresentationCore`：新增 `FGamePlatformHitFeedbackTuning`、`UGamePlatformHitFeedbackProfile`，参数包含轻击/重击/技能/格挡参考顿帧、暴击/破防可选增量、上限、闪白、摄像机/音效/特效、连击和本地受击强度。只存中立参数，无英雄或具体资源硬引用。
3. `GamePlatformAnimationClient`：新增 `UGamePlatformLocalHitstopSubsystem`。本地玩家作用域、0..10帧参考60Hz换算、命中GUID去重、重命中按较晚截止时刻覆盖、不加时长、定时恢复、清理旧世界；仅暂停Mesh骨骼动画，不暂停世界时钟、服务器GAS、移动碰撞和输入。
4. `GamePlatformVFX/SFX/Camera/UI/Input/Settings`：作为各自唯一执行器和设置入口；在后续P3/P6复用中立Presentation事件接线，不另建平行执行器。

### 第二层 MobaCommon：竞技反馈强度、命中分类

1. `MobaPresentationRuntime`：新增 `FMobaHitFeedbackPolicy`，将可信Contact类别Light/Heavy/Skill/Blocked/Missed（轻击、重击、技能、格挡、未命中）映射到中立强度/参考秒数。连击逐级增强但不延长顿帧；格挡强度骤降；挥空不触发接触反馈；暴击/破防额外帧受上限控制。
2. `MobaPresentationClient`：从已有Combat事件适配入口对已确认事实执行视觉顿帧；暴露 `ConfigureHitFeedback` 与按明确Contact/ComboStep执行的入口。当前旧事件不带攻击类别则临时按轻击，不得根据伤害数字猜测攻击类型。
3. `GamePlatformArena`：仍承担竞技权威上下文；后续真实网络事实通过既定GameplayCue或授权复制通道提供，不把C++ Delegate错当RPC。

### 第三层 DivineBeasts：项目配置与资产

1. `DBAClient/DivineBeastsPresentationRuntime`：新增 `UDivineBeastsCombatFeedbackCatalog`，通过HeroDefinitionId＋AbilityDefinitionId精确映射平台Profile软引用和VFX/SFX逻辑DefinitionId；重复键与空引用有中文错误提示。
2. `DBAClient/DivineBeastsPresentationClient`：后续经GamePlatformData租约异步预加载，并在项目内容包激活/切图/退服时注销。此模块不直接依赖Moba或竞技。
3. `DBAArena`客户端组合根：接线MobaPresentationClient并注入已加载Profile，竞技关闭时项目世界独立运行；不得反向让底层认识十二生肖。
4. `DBAHeroPack_*`：在实际UE编辑器制作各英雄动画、Niagara、音效与闪白材质资产，先用丑牛/寅虎/卯兔三种打击节奏进行主观评审；不得虚构技能ID或资源。

## 三、P0—P8实施步骤和状态

- [x] **P0：现状审查。** 已读三层规则和现有Combat、Presentation、Animation、Moba、项目客户端主要接口；保留已有未提交源码。
- [x] **P1：平台参数数据契约。** 已写入 `GamePlatformHitFeedbackProfile.h` 可编辑DataAsset类型。**待验证**UE反射与正式配置 .uasset、租约预加载。
- [x] **P2：局部视觉顿帧基础。** 已写入客户端LocalPlayer视觉停顿、帧率无关换算、去重、覆盖恢复及世界清理。**未完成**真实RootMotion安全隔离、击退表现积分暂停、0/3/6帧实际角色对比。
- [ ] **P3：完整九层表现。** 待在平台现有执行器中接入动画受击、SFX三层声音、Niagara短粒子、1–2帧目标闪白、可关屏幕震动、客户端表现击退及UI只读反馈，并完成同一命中GUID的分层时序与衰减策略。当前只完成数据契约及基础视觉顿帧。
- [x] **P4：MOBA强度算法。** 已写入 `MobaHitFeedbackPolicy.h/.cpp` 和 `MobaHitFeedbackPolicyTests.cpp`，配置0/3/6帧、暴击/格挡/挥空/连击/封顶判定；Moba客户端已调用平台视觉停顿。**待验证**真实技能类别和有效连击段数接入。
- [x] **P5：项目反馈目录类。** 已写入 `DivineBeastsCombatFeedbackCatalog.h/.cpp`。**待交付**真实生肖技能Profile DataAsset和内容包登记、项目组合根的加载/注入，非竞技世界需独立运行。
- [ ] **P6：输入、网络与击退。** 待在现有GamePlatformInput服务接入有限时效输入缓冲；在GameplayCue/受控复制通道定义权威事件GUID/角色代次和客户端预测修正；服务器负责真实击退，客户端顿帧不得改写权威移动与碰撞。
- [ ] **P7：自动化与性能验证。** 已写入MOBA策略测试源码，待UE真实执行。补充Mesh死亡/销毁/重生/切图测试，临时诊断界面和0/3/6帧对照；专项1v1—5v5性能/延迟压测，不编造阈值。
- [ ] **P8：编译、烘焙与运行验收。** UE5.8 Editor定向编译、Client/Server独立构建、Client与Dedicated Server最小Cook、双客户端联机、重连和人工打击感评审逐项记录；未实际跑过不标记通过。

## 四、技术安全门禁

1. 视觉顿帧必须局部且短促，不使用全局TimeDilation；不能冻结服务器Movement、GAS或其他单位。当前仅控制客户端SkeletalMesh `bPauseAnims`，不得误称已冻结根运动或实际位移。
2. 命中事件必须从权威来源或可撤回预测进入表现层；客户端重复GUID、跨World/Avatar代次事件不得重播。普通Delegate无法证明真实网络广播。
3. 顿帧期间输入仍采样并经上层合法性确认，输入缓冲有容量、有效期和取消窗口；不能无条件执行或重复消耗。
4. 所有特效、音效、贴图和材质统一由对应已存在的平台插件管理，异步租约来自GamePlatformData；服务器不加载客户端资源。
5. 资源缺失/提供者失效时安全降级；返回Gameplay结果不受表现错误影响。所有公共C++/Build.cs/测试/文档提供中文职责、边界与失败说明。
6. 编辑器Widget蓝图或其他真实UI资产只能通过Monolith MCP生成；`.uasset`、`.umap`必须是引擎真实产物。
7. 用户已有未提交文件不得覆盖。执行记录需真实注明源代码、UE构建、网络、Cook、视觉人工验收的各自结果。

## 五、后续正式验收所需场景

- 轻击/重击/技能/暴击/格挡/挥空，0/3/6帧对比；30/60/120fps刷新率差异。
- 一帧内重复GUID、多个命中覆盖（上限10）、目标销毁、动画原本已暂停、世界切换/多PIE、断线重连。
- 玩家受击和敌方受击的差异、输入采样/缓冲释放、真实击退和碰撞、根运动技能、镜头震动关闭、UI不被震动。
- 1v1至5v5的多角色高频命中，Niagara/SFX并发、CPU/GPU/内存和网络体积实际采样。
- Dedicated Server无纯客户端模块/资源烘焙或执行，Client/Server和Editor均真实编译、Cook、联机。

> 结果说明：本文描述的“已写入”仅表示源码改动，不等于已经通过UE编译、自动化测试、蓝图制作、Cook或多人真实运行。