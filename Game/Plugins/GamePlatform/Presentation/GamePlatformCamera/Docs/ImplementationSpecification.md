# GamePlatformCamera 实现规格

> 文档版本：1.0.0
>
> 更新日期：2026-09-29
>
> 设计状态：方案 A 已获确认，等待书面规格人工复核。
>
> 实现状态：尚未实施。本文中的类型、接口、资产、测试和验证要求均为实施合同，不是完成声明。

## 1. 目的与成功标准

GamePlatformCamera 是 GamePlatform 平台层的客户端相机机制插件。它向开放世界、竞技、观战、死亡、过场和其他上层组合提供稳定、项目中立、可测试的相机服务，同时复用 Unreal Engine 5.8 已有的 PlayerCameraManager、CameraModifier、CameraShake、ViewTarget 和 SpringArm 能力。

本设计采用“方案 A：平台稳定外观＋UE 原生相机适配”。完成后的最低成功标准是：

1. 每个 LocalPlayer 拥有独立的相机服务、模式账本、世界代次、数据租约和诊断快照。
2. 持续相机模式、玩家 Look 输入和瞬时镜头表现使用三个独立契约，不相互冒充。
3. 相机模式通过 GamePlatformData 的 Primary Asset（主资产）身份异步加载；相机插件不扫描目录、不硬编码文件路径、不建立第二个资产管理器。
4. 当前有效模式的选择结果可重复：先比较 Definition 优先级，再比较成功激活序列；不依赖数组、哈希、加载或注册顺序。
5. 旧世界、旧本地玩家代次或其他 LocalPlayer 签发的句柄不能修改当前世界。
6. 地图旅行、PlayerController 替换、Pawn 重生、CameraManager 重建、LocalPlayer 销毁和世界清理均有明确解绑、取消和释放顺序。
7. GamePlatformCamera 不依赖 MobaCommon、DivineBeasts、项目角色、项目地图、GamePlatformInput 或 GamePlatformUI。
8. 神兽联盟项目层只负责把输入和已确认的项目事实转换为平台相机请求，不复制相机算法。
9. Dedicated Server 最终产物不包含 Camera 客户端模块、相机资产或 Review Map。
10. 真实 Editor／Client 编译、Automation、Review、Client Cook、Server 构建与产物审计分别留证，任何单项结果不得冒充完整验收。

## 2. 当前真实基线

当前插件只有以下内容：

- GamePlatformCamera.uplugin：版本 0.1.0，CanContainContent=true，EnabledByDefault=false。
- GamePlatformCameraClient.Build.cs：仅依赖 Core。
- GamePlatformCameraClient.cpp：仅注册 FDefaultModuleImpl。
- README 和本规格文档。

当前不存在：

- Public 接口或反射类型；
- Camera Mode Definition；
- 模式栈或模式／效果句柄；
- LocalPlayer 相机子系统；
- PlayerCameraManager／CameraModifier 适配；
- GamePlatformData 租约；
- GamePlatformPresentation Provider；
- Input 或 DivineBeasts 组合接入；
- 自动化测试、Review Map、Cook 或目标构建证据。

当前也没有正式模块或目标依赖 GamePlatformCamera。因此，ClientOnly 只表达模块宿主意图，不能证明该模块已经进入客户端或已经从服务器最终产物中剥离。

## 3. 范围与非范围

### 3.1 平台相机负责

- LocalPlayer 作用域服务和隔离。
- 相机模式异步申请、取消、激活、抑制、释放和查询。
- 有硬容量上限的确定性模式栈。
- 模式间连续混入、混出和中断重定向。
- FOV、局部 Pivot、镜头距离、局部偏移、俯仰／偏航约束。
- 目标视觉跟随和目标丢失策略。
- 对 ViewTarget 已有 SpringArm 的复用。
- 明确启用时的有界 Pivot→Camera 碰撞 Sweep。
- Camera Shake／Impulse 的统一客户端入口和独立效果句柄。
- LookDelta 与 LookRate 的单位校验和最终消费。
- PlayerController、CameraManager 和世界生命周期适配。
- GamePlatformPresentation 的 Camera 通道提供者。
- 值快照、容量计数、失败结果和不含敏感身份的诊断。

### 3.2 平台相机不负责

- 服务器权威瞄准、命中、伤害、治疗、传送或目标合法性判断。
- 自动选择竞技目标、敌我判断或英雄规则。
- 网络复制、RPC、后端请求或玩家准入。
- 输入键位、重绑定、灵敏度持久化和输入设备识别。
- UI 层栈、焦点或模态窗口状态。
- MOBA 比赛状态机、神兽联盟角色类型或项目地图身份。
- 重新实现 PlayerCameraManager、CameraModifier、CameraShake、SpringArm 或 GameplayCameras。
- 通过相机朝向替代服务器需要的显式权威输入。

## 4. 层级、模块与依赖

### 4.1 保留单模块身份

插件继续只包含一个模块：

- GamePlatformCameraClient — ClientOnly。

一期不新增 Runtime、Core、Server 或 Editor 模块。Camera 的公开合同只供客户端组合消费；服务器不需要为了读取相机定义而链接任何 Camera 类型。

如果将来出现必须在无客户端模块条件下共享的真实合同，必须重新进行端侧影响评审，不为目录整齐预建空模块。

### 4.2 允许的依赖

GamePlatformCameraClient 的目标依赖如下：

- Public：Core、CoreUObject、Engine、GameplayTags、GamePlatformCore、GamePlatformData。
- Private：GamePlatformPresentationCore、GamePlatformPresentationClient。

插件描述文件需要显式声明对 GamePlatformData 和 GamePlatformPresentation 的插件依赖。

依赖原因：

- GamePlatformCore 提供统一 FGamePlatformResult。
- GamePlatformData 提供 UGamePlatformDefinitionBase、数据租约和 GameInstance 作用域服务。
- GamePlatformPresentationCore／Client 提供中立请求、目录解析和 LocalPlayer Provider 注册。
- Engine 提供 LocalPlayerSubsystem、PlayerController、PlayerCameraManager、CameraModifier、CameraShake、World 和碰撞查询。

### 4.3 禁止的依赖

GamePlatformCameraClient 不得依赖：

- MobaCommon 或 GamePlatformArena；
- DivineBeasts、DBAClient 或 DBAArena；
- GamePlatformInput；
- GamePlatformUI；
- GamePlatformSettings；
- 项目角色、技能、战斗或世界插件；
- UE 实验性 GameplayCameras 插件。

依赖方向固定为：

    DivineBeasts / MobaCommon / DBAArena
                    ↓
          GamePlatformCameraClient
                    ↓
       GamePlatformData + Presentation
                    ↓
             Unreal Engine Camera

GamePlatformPresentation 不反向依赖 Camera。Camera 作为具体播放器向 Presentation 注册 Provider。

## 5. 总体运行架构

    UGamePlatformCameraLocalPlayerSubsystem
        ├── IGamePlatformCameraService 公共门面
        ├── 模式请求账本（Pending / Active / Suppressed / Terminal）
        ├── 效果请求账本
        ├── GamePlatformData 租约
        ├── LocalPlayerGeneration
        ├── WorldGeneration
        ├── 当前 PlayerController / PlayerCameraManager 弱绑定
        ├── Presentation Provider 注册
        └── 私有 UGamePlatformCameraModifier
                    ├── 当前有效模式与过渡
                    ├── 目标快照
                    ├── FOV / Offset / Rotation
                    ├── 可选碰撞修正
                    └── 聚合性能计数

LocalPlayerSubsystem 是服务和所有权边界，但不自身建立第二条 Tick 链。每帧相机计算只进入绑定到当前 PlayerCameraManager 的私有 CameraModifier。

当没有有效模式时，Modifier 不修改基础 POV，UE 当前 ViewTarget／CameraComponent／SpringArm 的结果保持有效。这是缺少可选 Camera 内容时的安全回退。

## 6. 公开合同

### 6.1 服务发现

公开 IGamePlatformCameraService，并提供按 ULocalPlayer 查找的只读发现函数。发现失败返回 nullptr，不创建进程全局替身，不使用 GWorld，不默认获取第零号玩家。

UGamePlatformCameraLocalPlayerSubsystem 实现该接口，但内部栈、加载协调器、Modifier 和可变账本保持 Private。

所有接口只允许在游戏线程调用。跨线程调用返回明确失败或由开发断言捕获，不在内部静默切换线程。

### 6.2 模式申请

公开操作语义：

    FGamePlatformCameraModeHandle AcquireMode(
        const FGamePlatformCameraModeRequest& Request,
        FGamePlatformCameraModeCompletion Completion,
        FGamePlatformResult& OutResult);

FGamePlatformCameraModeRequest 固定包含：

- FPrimaryAssetId DefinitionId：GamePlatformCameraMode 主资产身份；
- FName SourceId：调用来源的稳定诊断身份，不参与排序；
- TWeakObjectPtr<UObject> Owner：拥有模式寿命的弱对象，不能为空；
- TWeakObjectPtr<UObject> TargetProviderObject：可空；非空时必须实现 IGamePlatformCameraTargetProvider。

调用方不能在请求中覆盖 Definition 优先级、碰撞策略或输入策略，避免同一内容身份在不同调用点产生不同运行规则。

规则：

1. 同步校验成功时返回有效句柄，初始状态为 PendingDefinition。
2. Definition 加载和模式激活异步完成；Completion 至多调用一次。
3. 同步拒绝返回无效句柄，并返回可检索的错误码。
4. Pending 句柄可以被 ReleaseMode 取消。
5. Definition 成功且请求仍属于当前世界、Owner 仍有效时，条目才进入 Active／Suppressed。
6. 模式申请只接受 World 期限；跨世界偏好不通过模式句柄保存。
7. 调用方需要跨图持续的体验时，必须在新世界激活事件中重新申请模式。

### 6.3 模式释放

    FGamePlatformResult ReleaseMode(
        const FGamePlatformCameraModeHandle& Handle);

规则：

- Pending 条目被取消，并释放其数据租约。
- Active／Suppressed 条目被移除；如果有效模式改变，从当前已输出 POV 混向新的有效模式或基础 POV。
- 同一合法句柄在终态保留窗口内重复释放返回成功且不重复产生副作用。
- 伪造、跨 LocalPlayer、跨世界或代次不匹配返回明确失败。
- 不提供按 DefinitionId、SourceId 或 Owner 批量删除的模糊公开接口。

### 6.4 Look 输入

    FGamePlatformResult SubmitLookInput(
        const FGamePlatformCameraLookInput& Input);

FGamePlatformCameraLookInput 必须包含：

- FVector2D Value；
- EGamePlatformCameraLookUnit Unit；
- float DeltaSeconds；
- FName SourceId。

单位只允许：

- DegreesDelta：Value 已是本次角度增量，不乘 DeltaSeconds。
- DegreesPerSecond：Camera 最终消费端乘且只乘一次 DeltaSeconds。

Camera 在当前输入策略允许时调用当前 PlayerController 的 AddYawInput／AddPitchInput；项目输入桥不得再执行第二次旋转。输入被模式禁用、Controller 缺失、单位错误、数值非有限或世界不匹配时返回失败，不缓存到未来世界。

输入灵敏度、反转和设备曲线仍由 GamePlatformInput 的单一真源产生。Camera 不保存第二份设置。

### 6.5 瞬时效果

    FGamePlatformCameraEffectHandle RequestEffect(
        const FGamePlatformCameraEffectRequest& Request,
        FGamePlatformCameraEffectCompletion Completion,
        FGamePlatformResult& OutResult);

    FGamePlatformResult StopEffect(
        const FGamePlatformCameraEffectHandle& Handle,
        bool bImmediately);

FGamePlatformCameraEffectRequest 固定包含：

- FPrimaryAssetId DefinitionId：GamePlatformCameraEffect 主资产身份；
- FGuid RequestId：预测、确认、纠正和取消使用的稳定去重身份；
- FName SourceId：调用来源诊断身份；
- TWeakObjectPtr<UObject> Owner：效果寿命所有者；
- float Scale：有限且非负的调用强度乘数，最终值仍受 Definition 和用户舒适度配置约束。

效果使用独立句柄，不复用模式句柄。Camera Shake 由当前 PlayerCameraManager 的原生能力执行；Camera 只负责 Definition 解析、生命周期、去重、停止和世界清理。

### 6.6 查询

服务提供值语义快照：

- FGamePlatformCameraSnapshot：当前有效模式身份、状态、世界代次、是否正在过渡、目标状态和输入策略。
- FGamePlatformCameraDiagnostics：Pending／Active／Suppressed／Effect 数量、容量拒绝、加载失败、过期句柄、目标丢失、碰撞查询／命中、模式切换和最近失败结果。

快照不得返回：

- 可变内部数组；
- Definition 裸指针；
- PlayerController／CameraManager 裸指针；
- 其他 LocalPlayer 的状态；
- 玩家账号、Token、后端 DTO 或敏感身份。

## 7. 句柄、状态与容量

### 7.1 模式句柄

FGamePlatformCameraModeHandle 包含：

- FGuid ScopeId：LocalPlayer 相机服务身份；
- FGuid HandleId：请求唯一身份；
- int64 LocalPlayerGeneration：子系统服务代次；
- int32 WorldGeneration：当前世界代次。

IsValid 只检查值形状；真实性、所有权和当前状态必须由服务账本验证。

### 7.2 效果句柄

FGamePlatformCameraEffectHandle 使用同样的 Scope、Handle、LocalPlayerGeneration 和 WorldGeneration 结构，但类型独立，禁止模式／效果接口互用。

### 7.3 模式状态

EGamePlatformCameraModeState 固定为：

- Invalid：从未签发或不能验证；
- PendingDefinition：已接纳，等待 Definition；
- Active：已激活且当前有效；
- Suppressed：已激活但被更高顺序模式覆盖；
- BlendingOut：已释放，仍由过渡快照贡献当前帧；
- Failed：加载或验证失败；
- Cancelled：激活前被取消或世界清理；
- Released：已完成释放。

Failed、Cancelled 和 Released 都是终态，不允许重新回到 Active。

### 7.4 容量

每个 LocalPlayer 固定：

- 最多 64 个 Pending＋Active＋Suppressed 模式请求；
- 最多 64 个 Pending＋Active 瞬时效果请求；
- 最多保留 128 条最近终态墓碑，用于短期幂等和诊断。

达到容量时同步拒绝新请求，不驱逐正在使用的条目，不自动扩容。终态墓碑采用固定容量 FIFO；墓碑被淘汰后，旧句柄返回 UnknownHandle，而不是伪造成功。

这些上限是防止错误调用无界增长的安全门禁，不是 CPU／GPU 性能预算。真实性能预算必须由实测确定。

## 8. Definition 设计

### 8.1 相机模式定义

UGamePlatformCameraModeDefinition 继承 UGamePlatformDefinitionBase，Primary Asset Type 固定为 GamePlatformCameraMode。

字段分组：

身份与选择：

- FGameplayTag ModeTag：平台中立模式语义；
- int32 Priority：模式栈主排序键；
- FName DiagnosticName：仅用于本地诊断，不作为选择键。

镜头：

- EGamePlatformCameraFovPolicy：Inherit 或 Override；
- float FieldOfViewDegrees：仅在 Override 时使用；
- FVector LocalPivotOffset；
- FVector LocalCameraOffset；
- float DistanceOffset；
- FGamePlatformCameraRotationLimits：可选俯仰／偏航约束。

混合：

- FGamePlatformCameraBlendSettings BlendIn；
- FGamePlatformCameraBlendSettings BlendOut；
- BlendSettings 包含非负时长和 UE 支持的确定性插值选项。

目标：

- EGamePlatformCameraTargetPolicy：None、LookAt、LockOn；
- bool bRequireTargetOnActivation：需要目标时，激活阶段缺失目标是否直接失败；
- EGamePlatformCameraTargetLossPolicy：FallbackUntargeted 或 ReleaseMode；
- 目标插值设置。

输入：

- EGamePlatformCameraInputPolicy：Free、Disabled、TargetRelative；
- 输入约束只决定 Camera 是否消费已归一化 Look 输入，不存键位、设备或用户设置。

碰撞与延迟：

- EGamePlatformCameraCollisionPolicy：RespectViewTarget、Disabled、SweepFromPivot；
- EGamePlatformCameraLagPolicy：RespectViewTarget、Disabled、Exponential；
- SweepFromPivot 使用定义中的有限正半径、Trace Channel 和过滤策略。

ValidateDefinition 必须先调用 Super，再检查：

- ModeTag 有效；
- 所有浮点数有限；
- 时长非负；
- FOV 满足 0 < FieldOfViewDegrees < 180，避免透视投影奇点；
- 最小旋转限制不大于最大限制；
- SweepFromPivot 半径为有限正值；
- 需要目标的策略与目标丢失策略组合有效；
- 不包含项目类、硬资源路径或服务器类型。

### 8.2 瞬时效果定义

UGamePlatformCameraEffectDefinition 继承 UGamePlatformDefinitionBase，Primary Asset Type 固定为 GamePlatformCameraEffect。

一期只封装经过验证的 UCameraShakeBase 软类引用及以下策略：

- 默认强度；
- 播放空间；
- 可选用户舒适度类别；
- 同一来源／RequestId 的去重语义；
- 停止策略。

软引用标记到 Camera Asset Bundle，由 GamePlatformData 租约负责加载。Camera 不在每帧路径同步加载 Shake 类。

### 8.3 内容所有权

GamePlatformCamera 保留 CanContainContent=true，允许拥有：

- 平台中立基础模式定义；
- 平台中立效果定义；
- Camera Review Map 和评审夹具。

以下内容不得归平台 Camera：

- DivineBeasts 英雄专属定义；
- OpenWorld／Village／MainArena 世界专属定义；
- DBAArena 竞技演出定义；
- 项目皮肤、角色或地图硬引用。

项目内容放入实际登记的项目内容包。Review 内容必须由 UE 编辑器真实生成并保存，禁止文本占位或改扩展名伪造；Shipping 与 Server Cook 是否包含它们必须通过实际产物审计控制。

## 9. 目标提供者

公开 IGamePlatformCameraTargetProvider，仅提供只读视觉目标快照，不提供权威选择能力。

TryBuildCameraTargetSnapshot 必须：

- 仅在游戏线程调用；
- 以 TWeakObjectPtr<UWorld> 返回目标所属世界，不让调用方跨帧持有世界裸指针；
- 返回稳定 Pivot 位置和可选朝向；
- 返回本帧是否有效；
- 不返回网络权限、伤害判定或服务器可见性结论。

模式请求以弱对象持有 Provider。激活和每帧使用前校验：

- Provider 仍有效；
- Provider 属于当前 GameInstance；
- 需要目标时属于当前 World；
- 坐标和旋转为有限值。

激活时缺少目标且 bRequireTargetOnActivation=true，整个模式请求失败并释放租约；成功激活后目标丢失时严格执行 Definition 的 TargetLossPolicy。Camera 不自动搜索替代敌人。

Owner 与 TargetProvider 可以是不同对象：Owner 管理模式寿命，TargetProvider 只提供视觉目标。

## 10. 确定性模式栈

### 10.1 排序

只有成功激活的条目参与选择。排序规则固定为：

1. Definition.Priority 降序；
2. ActivationSequence 降序，即相同优先级后激活者优先。

ActivationSequence 只由当前 LocalPlayer 服务在成功激活时递增签发，不使用哈希、指针、资产名称或异步完成的容器遍历顺序作为隐式决胜。

Pending 条目不影响当前镜头。加载完成后，如果请求仍有效，才获得 ActivationSequence。

### 10.2 重复请求

同一个 Definition、SourceId 或 Owner 可以拥有多个独立模式句柄。每个句柄必须单独释放。

一期不提供 Replace 或按标签批量 Pop：

- 需要无缝替换时，调用方先 Acquire 新模式；
- 新模式成功激活后再 Release 旧模式；
- 新模式失败时旧模式保持不变。

该事务顺序避免替换失败导致基础相机意外暴露。

### 10.3 混合与中断

有效模式变化时：

1. 采样当前已经输出的 FMinimalViewInfo，形成不持有 UObject 的过渡起点快照；
2. 读取新有效模式目标；
3. 新模式成功激活并抢占当前模式时使用新模式的 BlendIn；当前模式被释放、Owner 失效或目标丢失而回到下层／基础 POV 时使用离开模式的 BlendOut；世界清理和 LocalPlayer 销毁不执行视觉混合；
4. 每帧只推进当前一条过渡；
5. 过渡中再次切换时，从当前输出快照重定向，不回跳到旧模式初值；
6. 目标模式在过渡中失效时，立即重新选择有效模式并从当前输出重定向。

Definition 对象只在其数据租约有效时读取。过渡需要的数据必须复制为值，释放租约后不再访问旧 Definition。

### 10.4 Owner 失效

- 当前有效模式的 Owner 每帧只做一次弱有效性检查。
- 非有效条目在模式变更、世界切换和 PostGarbageCollect 后进行有界压缩。
- Owner 失效等价于调用方释放；Pending 请求取消，Active／Suppressed 请求移除并释放租约。
- 不为 Owner 建立强引用，不延长项目对象寿命。

## 11. GamePlatformData 集成

每个模式或效果申请使用当前 GameInstance 的 IGamePlatformDataService：

1. 用 FPrimaryAssetId 和期望 Definition 类申请 World 期限租约；
2. Bundles 使用 Core 和 Camera 去重集合；
3. WeakCaller 使用当前 Camera LocalPlayerSubsystem；
4. 同步拒绝不进入相机账本；
5. 异步完成后重新校验 ScopeId、LocalPlayerGeneration、WorldGeneration、HandleId、Owner 和取消状态；
6. 成功后通过 GetLoadedDefinition 取得只读对象并复制运行所需值；
7. 失败或校验不通过时回滚相机条目并释放租约；
8. 每个相机请求只释放自己的租约，不卸载其他世界或调用方仍在使用的资源。

Camera 不修改 UGamePlatformDefinitionBase，不缓存跨 GameInstance 裸指针，也不依赖文件夹存在自动注册资产。

## 12. UE 原生相机适配

### 12.1 PlayerController 与 CameraManager

UGamePlatformCameraLocalPlayerSubsystem 覆盖 PlayerControllerChanged：

- 解除旧 CameraManager 上的 Modifier 和效果绑定；
- 更新弱 Controller；
- 读取新的 PlayerCameraManager；
- 按当前世界重新建立私有 Modifier；
- 不重放旧世界模式；
- 不在模块 StartupModule 中生成玩家或连接世界。

RefreshBinding 在以下事件执行：

- Subsystem 初始化；
- PlayerControllerChanged；
- 世界代次变化；
- 模式／效果请求；
- SubmitLookInput。

如果同一 PlayerController 的 CameraManager 发生替换，下一次 RefreshBinding 迁移到新 Manager；旧 Modifier 失效时不得继续输出。

### 12.2 CameraModifier

UGamePlatformCameraModifier 位于 Private：

- 由当前 PlayerCameraManager 创建和移除；
- 每帧只读取子系统发布的不可变运行快照；
- 不加载资产；
- 不注册网络或后端回调；
- 不持有项目角色强引用；
- 不对外公开可变指针。

Modifier 负责：

- 应用当前过渡；
- 应用 FOV、局部 Pivot／Camera Offset 和旋转约束；
- 使用有效目标快照；
- 必要时执行一次 SweepFromPivot；
- 更新聚合性能计数。

### 12.3 SpringArm 与碰撞

碰撞策略固定为：

- RespectViewTarget：相信当前 ViewTarget／CameraComponent／SpringArm 已完成碰撞和延迟，Camera 不执行第二次 Sweep。
- Disabled：Camera 只应用定义变换，不做碰撞。
- SweepFromPivot：TargetProvider 或当前 ViewTarget 提供 Pivot，Camera 从 Pivot 向期望位置执行一次 Sphere Sweep，并将镜头收缩到安全点。

插件不永久修改任意 Pawn 的 SpringArm 配置，不复制 SpringArm 的全部 Lag 和 Collision 算法。

### 12.4 Camera Shake

效果通过当前 PlayerCameraManager 的原生 StartCameraShake／StopCameraShake 执行。句柄记录弱实例和数据租约；Manager 替换、世界退出、效果取消或 Owner 失效时停止自身创建的 Shake。

Camera 不停止其他系统直接创建的未知 Shake。项目实施完成后，正式项目代码不得绕过 Camera 服务直接创建同类 Shake。

## 13. Presentation 集成

Camera LocalPlayerSubsystem 向 UGamePlatformPresentationClientSubsystem 注册：

- ProviderId：Camera；
- ProviderChannel：Camera；
- Priority：100，与既有 VFX Provider 使用相同领域提供者优先级；不同通道必须先自行拒绝，不能依赖字典序抢占；
- Deinitialize 时使用注册句柄注销。

Presentation Provider 只接受可选的瞬时／定时相机表现，例如：

- Camera.Impact；
- Camera.Explosion；
- Camera.Death.Impulse；
- Camera.Arena.Intro.Impulse。

规则：

1. ProviderChannel 不是 Camera 时立即返回 false，让其他领域 Provider 继续处理。
2. Catalog 的 DefinitionId 必须是可严格解析的 GamePlatformCameraEffect Primary Asset Id 字符串，不是文件路径。
3. Instant 和 Timed 请求转换为 RequestEffect。
4. Persistent 请求返回未处理；持续状态必须由拥有明确 Owner 的 AcquireMode 建立。
5. WorldGeneration 不匹配返回 StaleWorld。
6. RequestId 用于去重；Predicted、Confirmed、Corrected、Cancelled 按 Presentation 合同处理，不能重复播放确认事件。
7. Provider 失败只影响表现，不改变服务器权威结果。

Camera 不在平台层硬编码 DivineBeasts 语义标签。项目或 MobaPresentation 注册 Catalog Fragment，把项目语义映射到 Camera 通道和 Definition 身份。

## 14. GamePlatformInput 与 DivineBeasts 接入

### 14.1 平台层边界

GamePlatformCamera 不依赖 GamePlatformInput。GamePlatformInput 继续生产带单位的中立事件。

调用方向是：

    GamePlatformInputClient
            ↓
    DivineBeastsInputClient
            ↓
    IGamePlatformCameraService::SubmitLookInput

### 14.2 项目输入迁移

DivineBeastsInputClient 实施时：

- 增加对 GamePlatformCameraClient 的实现依赖；
- 将 LookDelta／LookRate 事件转换为 FGamePlatformCameraLookInput；
- 删除同一路径直接调用 AddYawInput／AddPitchInput 的旧消费；
- LookRate 不再在项目桥乘 DeltaSeconds，由 Camera 最终消费；
- Move、Ability 和 UI 输入路径保持原职责，不因 Camera 重构扩散；
- Camera 服务缺失时记录有界诊断并拒绝 Look，不恢复第二条隐式旋转真源。

TargetLock 输入仍然是项目游戏命令，不直接让 Camera 选择目标。Camera 只消费玩法系统已经确认或复制的目标事实。

### 14.3 项目相机组合

在现有 DivineBeastsPresentationClient 模块内增加一个职责单一的 LocalPlayer 项目相机适配子系统，不新增插件或空模块。该适配层：

- 监听应用、世界、角色、死亡、观战、竞技和已确认目标事实；
- 选择项目内容包中的 Camera Mode Definition Id；
- 持有并释放平台模式句柄；
- 注册项目 Presentation Catalog Fragment；
- 不实现混合、碰撞、Shake 或目标搜索算法。

DBAArena 或 MobaPresentation 可以产生中立竞技表现语义，但平台 Camera 不反向认识竞技。关闭 MOBA／DBAArena 后，项目通用世界相机仍可运行。

## 15. 生命周期与清理

### 15.1 初始化

LocalPlayerSubsystem 初始化只执行：

- 创建 ScopeId 和 LocalPlayerGeneration；
- 绑定 World Cleanup；
- 初始化固定容量账本；
- 初始化 Presentation 依赖并注册 Provider；
- 刷新 Controller／CameraManager 绑定。

不执行登录、地图加载、Pawn 生成、生产后端连接或项目模式申请。

### 15.2 世界变化

检测到新 World 时：

1. 停止接纳旧世界新请求；
2. 递增 WorldGeneration；
3. 标记所有 Pending 为 Cancelled；
4. 撤销所有 Active／Suppressed 模式；
5. 停止本服务创建的效果；
6. 移除旧 CameraManager Modifier；
7. 解绑旧世界委托；
8. 释放全部相机数据租约；
9. 清空过渡和目标值快照；
10. 绑定新世界和新 CameraManager；
11. 恢复接纳新世界请求。

旧模式不自动跨世界重放。项目层在新世界 Ready 事件后重新申请所需模式。

### 15.3 Deinitialize

Deinitialize 按以下顺序：

1. 标记 ShuttingDown，拒绝新请求；
2. 注销 Presentation Provider；
3. 取消 Pending；
4. 停止效果；
5. 移除 Modifier；
6. 解绑 Controller／World／GC 委托；
7. 释放租约；
8. 清空账本和快照；
9. 使 ScopeId 失效并递增服务代次。

迟到回调只能看到代次不匹配并结束，不得重新发布状态。

## 16. 错误、取消与降级

所有公开失败使用 FGamePlatformResult，错误码稳定、可检索、中文消息不包含对象地址或敏感信息。

最低错误码：

- CameraServiceUnavailable；
- CameraShuttingDown；
- CameraInvalidRequest；
- CameraInvalidNumericValue；
- CameraWrongThread；
- CameraWrongScope；
- CameraWrongWorld；
- CameraStaleHandle；
- CameraUnknownHandle；
- CameraCapacityExceeded；
- CameraDefinitionRejected；
- CameraDefinitionLoadFailed；
- CameraDefinitionTypeMismatch；
- CameraOwnerExpired；
- CameraTargetUnavailable；
- CameraControllerUnavailable；
- CameraManagerUnavailable；
- CameraPresentationRequestRejected。

降级原则：

- 没有有效模式：保持 UE 基础 POV。
- 可选效果失败：不影响玩法。
- 目标丢失：执行 Definition 明确策略。
- Camera 服务缺失：项目 Look 请求失败并产生诊断，不悄悄建立第二套控制路径。
- 必需的基础相机定义缺失：项目层可拒绝进入需要该体验的客户端阶段，但不得伪造成功；服务器权威世界仍不依赖 Camera。

## 17. 性能与内存边界

### 17.1 每帧允许的工作

每个活跃 LocalPlayer 每帧仅允许：

- 一次当前有效模式／过渡评估；
- 一次当前目标快照校验；
- 至多一次显式 SweepFromPivot 碰撞查询；
- 当前原生 Camera Shake 的 UE 内部更新；
- 无分配的聚合计数更新。

### 17.2 每帧禁止的工作

- Primary Asset 加载或同步软引用加载；
- 目录扫描或 Catalog 全量重建；
- 全量模式排序；
- UObject 创建；
- 动态字符串拼接和无界日志；
- 无变化的 Blueprint／C++ 委托广播；
- 网络、HTTP 或后端调用；
- UI 状态轮询；
- 对所有候选模式分别做碰撞或目标搜索。

模式栈只在申请成功、释放、Owner 清理、世界变化或 Definition 终态时重新选择。每帧读取已经发布的值快照。

### 17.3 测量

一期必须为 Unreal Insights／CSV 提供：

- EvaluateCamera；
- AdvanceTransition；
- CollisionQuery；
- ActiveModeCount；
- PendingDefinitionCount；
- ActiveEffectCount；
- TargetLostCount；
- CapacityRejectedCount。

本文不伪造 CPU、GPU 或内存阈值。性能报告必须写明机器、平台、构建配置、地图、玩家数、分辨率、帧率目标、采样区间和实测结果，再由项目批准预算。

## 18. 网络、安全与隐私

- 插件完全客户端，不声明 Server RPC、Client RPC 或复制属性。
- 相机目标来自上层已确认事实；Camera 不把屏幕中心、客户端射线或当前 POV 当作服务器命中证据。
- 相机模式和效果请求不携带 Token、支付、账号或后端 DTO。
- 诊断只记录模式标签、Definition 身份、世界代次、错误码和聚合计数；不记录玩家姓名、账号、聊天、设备唯一标识或精确行为轨迹。
- Definition 使用 Primary Asset Id 和软引用，不接受任意文件系统路径。
- 数值输入先做 IsFinite 和范围校验，拒绝 NaN、Infinity 和非法时长。
- 公开 API 不返回可跨帧保存的引擎裸指针。

## 19. 设置与可访问性边界

- 输入灵敏度、反转和设备曲线仍归 GamePlatformInput。
- Camera Definition 保存内容设计参数，不保存用户个人设置。
- Motion Reduction、Shake Scale、FOV Comfort 等长期偏好最终归 Camera 领域，但在 GamePlatformSettings 完成单一真源迁移前，一期不在 Camera 与 Settings 中重复持久化。
- 一期允许项目组合通过只读运行配置降低或关闭可选 Shake；该值不写回配置、不伪装成已完成的用户设置系统。
- UI 通过 Input Block 和项目相机模式句柄协调，不让 Camera 依赖 Widget 或轮询焦点。

## 20. 测试设计

### 20.1 纯策略 Automation

必须先编写失败测试，再实现：

- 不同优先级选择；
- 相同优先级后激活者优先；
- Pending 不参与选择；
- 释放顶层后恢复下层；
- 新模式加载失败时旧模式保持；
- 重复独立申请和单句柄释放；
- 64 条容量边界和第 65 条拒绝；
- 终态墓碑容量；
- 跨 Scope／LocalPlayerGeneration／WorldGeneration 拒绝；
- Pending 取消和迟到完成抑制；
- 混合中断连续性；
- Owner 失效清理；
- 目标激活必需条件和 TargetLossPolicy 两种运行结果；
- LookDelta 不乘 DeltaSeconds；
- LookRate 只乘一次 DeltaSeconds；
- 非有限数值拒绝。

### 20.2 UE 集成 Automation

- LocalPlayerSubsystem 创建与销毁；
- 两个 LocalPlayer 隔离；
- PlayerControllerChanged；
- Pawn 重生；
- CameraManager 重建；
- 世界旅行和 World Cleanup；
- Modifier 安装／移除；
- SpringArm RespectViewTarget；
- SweepFromPivot 命中和未命中；
- Camera Shake 启动、停止和 Manager 替换；
- Data 成功、失败、取消、类型不匹配和 Owner 过期；
- Presentation Provider 注册、注销、WorldGeneration、去重和取消；
- 项目 Look 输入只有一个最终旋转消费者。

### 20.3 架构静态门禁

新增 Camera 专项脚本，检查：

- 模块和插件依赖不反向指向 MobaCommon／DivineBeasts；
- 不依赖 GamePlatformInput／UI／Settings／GameplayCameras；
- 公开目录不包含私有栈、Modifier、加载器或 CameraManager 可变实现；
- 不出现 GWorld、GetFirstPlayerController 或固定第零号玩家；
- 不声明网络 RPC 或复制属性；
- GamePlatformCameraClient 仅在允许的目标出现；
- 项目输入旧直接旋转路径已删除；
- 新增一方代码包含职责充分的中文说明。

静态门禁只是辅助证据，不能替代编译、运行或人工评审。

## 21. Review Map 与人工评审

使用 UE 编辑器真实创建 Camera Review Map，至少包含：

- Exploration；
- Combat；
- Lock-on 和目标丢失；
- Arena Intro；
- Spectator 切换；
- Death；
- 室内窄通道；
- 贴墙和遮挡；
- 高速移动和快速转向；
- UI 模态打开／关闭；
- 鼠标与手柄；
- 30／60／120 FPS；
- 多分辨率和安全区；
- 两个 LocalPlayer；
- 地图旅行后旧句柄失效；
- Motion Reduction／Shake Scale 运行配置。

评审记录必须包含：

- BuildVersion；
- ContentRevision；
- 地图和 Definition Id；
- 测试步骤；
- 输入设备；
- 构建配置；
- 实际观察；
- 失败日志；
- 已知限制。

Review Map 通过只证明视觉和交互观察，不证明 Server 剥离、Cook、网络权威或性能预算。

## 22. 构建、Cook 与交付门禁

实施完成后分别执行并记录：

1. Camera 纯策略与 UE Automation；
2. GamePlatformCameraClient Editor Development 编译；
3. DivineBeastsArena Client Development 编译；
4. DivineBeastsArena Client Shipping 编译；
5. 既有普通 Game 目标编译；
6. Dedicated Server 目标编译；
7. 干净 Client Cook／Stage；
8. Server Cook／Stage 或等价产物审计；
9. Review Map 人工评审；
10. Unreal Insights 性能采样。

服务器产物审计必须确认不包含：

- GamePlatformCameraClient 二进制；
- Camera Mode／Effect Definition；
- Camera Shake；
- Camera Review Map；
- 项目相机表现资产。

任一环境不可用时必须如实记录未执行项、原因和风险。

## 23. 目标源码布局

实施后的建议布局：

    Source/GamePlatformCameraClient/
    ├── GamePlatformCameraClient.Build.cs
    ├── Public/
    │   ├── Definitions/
    │   │   ├── GamePlatformCameraModeDefinition.h
    │   │   └── GamePlatformCameraEffectDefinition.h
    │   ├── Interfaces/
    │   │   ├── IGamePlatformCameraService.h
    │   │   └── IGamePlatformCameraTargetProvider.h
    │   ├── Subsystems/
    │   │   └── GamePlatformCameraLocalPlayerSubsystem.h
    │   └── Types/
    │       ├── GamePlatformCameraHandles.h
    │       ├── GamePlatformCameraRequests.h
    │       ├── GamePlatformCameraResults.h
    │       └── GamePlatformCameraSnapshot.h
    └── Private/
        ├── Diagnostics/
        ├── Loading/
        ├── Modes/
        ├── Modifiers/
        ├── Presentation/
        ├── Tests/
        └── GamePlatformCameraClient.cpp

规则：

- Public 只保留调用方必须依赖的稳定合同。
- Mode Stack、Transition、Definition Coordinator、Modifier、Provider 和诊断聚合实现均在 Private。
- 编译测试位于 Private/Tests。
- 不新增无职责的 Manager、Helper、Util、Provider 或 Subsystem。
- 所有人工维护代码文件必须有中文职责、端侧、调用方、依赖、所有权和生命周期说明。

## 24. 实施分期

### 阶段 1：合同与纯策略

- 类型化句柄、请求、结果、快照；
- Definition C++ 类型及验证；
- 固定容量模式策略；
- 过渡状态机；
- 纯策略失败测试和实现。

阶段出口：纯逻辑测试覆盖正常、失败、取消、容量和代次，不接入项目。

### 阶段 2：Data 与 LocalPlayer 服务

- Data 租约；
- 异步加载、取消和迟到回调保护；
- LocalPlayerSubsystem；
- 世界代次和固定容量账本；
- 服务发现和诊断。

阶段出口：Data／生命周期 Automation 通过。

### 阶段 3：UE 相机适配

- PlayerControllerChanged；
- CameraManager 绑定；
- 私有 CameraModifier；
- 混合、目标、FOV、Offset、碰撞；
- 原生 Camera Shake 效果。

阶段出口：UE 集成测试和最小 Review 场景通过。

### 阶段 4：Presentation 与项目输入迁移

- Camera Provider；
- Presentation 去重／取消；
- DivineBeastsInputClient Look 单一消费迁移；
- 项目相机适配子系统；
- 项目／MOBA Catalog Fragment。

阶段出口：不存在旧直接 Look 消费；平台无项目反向依赖。

### 阶段 5：内容、Review 与交付

- 平台中立定义；
- 项目内容包定义；
- Review Map；
- 架构门禁；
- Editor／Client／Server 构建；
- Client Cook；
- Server 产物审计；
- 性能采样和人工中文审核。

阶段出口：所有已执行证据可追踪，未执行项明确记录。

## 25. 文档同步要求

实施计划和后续代码修改必须同步：

- 本 README；
- 本 ImplementationSpecification；
- GamePlatformCamera.uplugin；
- 游戏端插件清单设计；
- 游戏端插件系统 P0 收敛审计；
- 解决方案总体目录规划说明；
- 插件开发规范中发生变化的明确边界；
- DBAClient 输入／表现说明；
- 新增 API、测试、Review 和验证证据文档；
- Docs/CHANGELOG.md。

全局文档当前存在其他未提交修改，后续必须以最小差异合并，禁止覆盖无关工作。

## 26. 明确拒绝的替代方案

### 26.1 不直接硬依赖 UE GameplayCameras

UE 5.8 的 GameplayCameras 功能丰富，但仍为实验性，并会引入 EnhancedInput、StateTree、TemplateSequence 和第二套相机运行模型。一期不将其设为硬依赖。

未来如需使用，只能在 IGamePlatformCameraService 后增加私有执行适配器，并独立验证引擎版本、Cook、性能、输入重复和资产迁移。

### 26.2 不只实现项目 PlayerCameraManager 子类

仅在 DivineBeasts 中实现项目专属 PlayerCameraManager 虽然初期较快，但会绕过平台插件、耦合 GameMode／PlayerController、削弱复用，并违反三层边界，因此拒绝作为正式实现。

### 26.3 不把持续模式全部建模为 Presentation 请求

Presentation 表现允许失败和降级，不拥有探索、观战、死亡等持续状态的业务寿命。持续模式必须由明确 Owner 持有模式句柄；Presentation Provider 只处理可选瞬时／定时表现。

## 27. 规格评审检查表

在进入实施计划前，人工评审必须确认：

- 平台／MOBA／项目依赖方向无反转；
- 单模块 ClientOnly 决策可接受；
- 模式、Look、效果三个合同分离；
- 模式排序和重复申请规则明确；
- 句柄字段、状态和终态规则明确；
- Data 租约、取消和迟到回调规则明确；
- Controller／CameraManager／World 清理顺序明确；
- SpringArm、碰撞和 Shake 复用边界明确；
- Presentation Persistent 请求被明确拒绝；
- 项目输入只保留一个最终旋转消费者；
- 容量和诊断不无界增长；
- 测试、Review、Build、Cook 和 Server 剥离门禁可以独立验收；
- 文档没有把计划能力描述成已实现。

书面规格复核通过后，下一步才使用实施计划流程形成逐文件、逐测试、逐验证命令的执行计划；在实施计划再次获得确认前不修改产品代码。
