# Village lake underwater and swimming implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有原生新手村湖泊中实现双端游泳/自由潜水及对应水下光学。
**Architecture:** 原生Water负责湖面和最终相机水下检测；现有WaterRegion绑定同源烘焙PhysicsVolume，CharacterMovement负责唯一网络运动。Surface持有通用材质，Village持有具体参数，沿用事件通知及唯一VFX/SFX链。
**Tech Stack:** UE5.8、C++、UMG/Animation、Monolith Python、PowerShell。
**Spec:** `Docs/Implementation/Water/VillageLakeUnderwaterSwimDesign_20261031.md`，用户已明确批准继续实施并选择不做氧气/溺水。

## Global Constraints

- 原位执行，保留当前用户地图/AI/植被/字体工作；不创建第二工程，不提交或推送。
- 不雕刻地形，不更改湖岸、河流、出生点与既有PCG排除。目标脏包不覆盖，修改前备份。
- 原生资源只经正式工程Monolith创建、编译、保存及独立重载；所有输出在工程内。
- Runtime不得依赖Water/VFX/Surface/UI；专服只保留轻量CPU水域。输入走原生CharacterMovement，不提交位置RPC。
- 固定增量参数Win64/Development、NoSharedPCH、NoUBA、MaxParallelActions 8，Editor另UsePrecompiled及AdditionalPlugins DBAServer。
- 调试t.MaxFPS 0、VSync 0，核实际状态；不保存玩家偏好。联机前同步真实Client/Village/MainArena合同。

## Review Focus

- 凹岸、高地、水线和湖底不能被矩形范围错误包为泳区；物理体积与Region查询必须同源。
- 水面查询碰撞不能阻挡Pawn、改变导航或雕刻地形；相机进出水不能使用角色脚部代替。
- 水域失效、跨世界引用、空Brush不得回退到宽Box伪造成功；旧Box兼容仅显式未迁移时保留。
- 泳潜输入失去焦点、死亡、失控及出水后不得留下升降输入；动画只读移动事实。
- 专服WaterBody剥离后PhysicsVolume和Region仍完整；视觉资源失败不改变运动。

### Task 1: 原生基线与失败验证

**Files:** `Tools/Unreal/World/VillageWaterSystemsMonolith.py`；证据 `Saved/Validation/VillageWaterSystems20261031`。
**Interfaces:** `inspect_world()` 记录Lake身份/碰撞/材质、Region、物理体积、骨架动作；`verify_world()` 对真实泳区与查询合同断言。

- [ ] 启动唯一正式编辑器，原生读取Village和现有WaterReview；保存精确指纹和只读报告。
- [ ] 在当前缺失物理水域和水下查询的真实基线上运行verify，观察明确失败，保留日志。

### Task 2: 双端水域、泳潜输入与动画事实

**Files:** `GamePlatformWaterRuntime/Public/Actors/GamePlatformWaterRegion.h` 及Private实现/Tests；`GamePlatformWaterEditorLibrary.h/.cpp`；`DivineBeastsGameplayCharacter.h/.cpp`；`DefaultInput.ini`；`GamePlatformLocomotionAnimInstance.h/.cpp`及既有Tests。
**Interfaces:** Region新增 `WaterPhysicsVolume` 与显式要求精确体积的配置，SampleRegion优先查询该体积；Editor提供 `BakeLakePhysicsVolume(AActor*, AGamePlatformWaterRegion*, float, FString&)`；角色 `SwimVertical(float)` 只在Swimming时提交原生加速度；动画 `bIsSwimming` 反映实际移动模式。

- [ ] 添加实际失效体积/凹岸/进出水和输入资格回归；对当前实现观察失败。
- [ ] 实现现有Region的原生体积接线及同源作者，烘焙闭合Brush，拒绝跨世界/空体积/歧义；保留显式旧Box兼容。
- [ ] 完成原生泳潜参数、W/S视线运动、Space/Ctrl升降及只读动画字段，保持地面输入和权威死亡/失控门禁。
- [ ] 完整Editor增量构建并运行相关原生回归，所有必要用例通过。

### Task 3: 真实湖体、水下材质及动画资产

**Files:** 同一Monolith工具、通用Water材质/函数、Village稳定材质实例、现有审核地图和Village地图、当前主线角色动画资产。
**Interfaces:** 消费Task2的原生体积作者；相机使用WaterSubsystem；具体材质参数/纹理由真实反射决定，保证其他世界默认行为。

- [ ] 通过Monolith备份、建立精确水域、恢复湖体QueryOnly水下检测，保存后回读泳区和湖体边界不变。
- [ ] 完成同一水面/水下链的吸收散射、局部动画焦散及四档策略，编译Shader；绑定稳定Village实例。
- [ ] 原生审查当前骨架与动作，复用或制作基础泳姿并接入现有动画图，编译保存。
- [ ] 独立新进程重载，verify由RED转GREEN；固定镜头截图及动态泳潜/水线证据。

### Task 4: 构建、Cook、联机与交付

**Files:** 当前构建/Cook/启动脚本和必要精确CustomConfig、交付文档；不复制执行器。
**Interfaces:** 实际Client/Server构建输入、包、PID与角色；原生容器清单和运行日志分别记录。

- [ ] 受影响Editor/Client/Server完整增量目标通过；精确Cook/Stage，实际容器检查CPU体积保留/纯表现剥离。
- [ ] 核对Village与MainArena真实运行版本，必要时同步/受控重启，再执行双端泳潜与清理回归。
- [ ] 一次fresh整体代码审查，修复实质问题，分别报告原生、自动化、运行、GPU和人工签收；不代填人工通过。

## Execution ledger

2026-10-31：用户批准设计、要求继续实施，明确不做氧气/溺水。沿用上一阶段已批准的原位Native执行方法，不重复询问实施方式或计划批准；当前main有既存多任务编辑，不能移到另一工程破坏原生资源引用。`.git`只读，不提交。每任务在本文件记录实际结果及偏离裁定。

Pre-flight：Task2产生的WaterPhysicsVolume/Bake接口由Task3消费；Task3保存的World内容由Task4 Cook消费。头部浸没若没有真实消费者暂不增加复制字段，泳态以CharacterMovement原生复制为准；避免不必要复制合同扩张。

Task1：PID46820原生读取Village，验证按预期失败“缺少双端原生Water PhysicsVolume”；实际无Region、无PhysicsVolume、湖体无查询碰撞，注册表89个AnimSequence无泳姿候选。退出前脏包0。既有18,000点地形调查在湖岸内包含7,714采样，最低Z=-2050.013638cm，故Village物理水深采用2500cm，避免原1500cm体积在湖底以上结束。

Task2：首次沙盒.NET构建异常退出(-532462766)，同参数本机构建bb133014实际进入35动作；失败原因定位为测试调用了UE5.8不存在的Axis getter，以及Editor作者缺MaterialInterface完整头，已按锁定源码修正。未改变PCH/目标/工具链参数。

Task2 Ruling：983a178a暴露UEditorBrushBuilder的15项未导出虚函数，不能直接跨DLL派生使用；改从Engine公开UBrushBuilder填写原生Model/Polys，再由已有UActorFactory进行BSP/BodySetup烘焙，不复制BSP或碰撞执行器。

Task2：0de10965完整Editor构建通过；原生凹岸几何/失效体积断言通过，但测试在未初始化运动的Editor世界中未触发泳态，实际1项失败；泳潜输入原生测试通过（1项既有GAS GameplayCue扫描warning）。测试改在隔离Game世界构造Actor并显式接入UpdatedComponent，只在受限作者同步栈标记Editor。当前打开的审核图被测试标脏，未保存，关闭并从磁盘恢复；不将测试世界状态写回真实地图。补充非有限查询的Physics前置拒绝。

Task2完成：0b452d96完整Editor增量构建通过；PID50080原生执行GamePlatform.Water两项、GamePlatform.Animation.Locomotion两项均通过且0warning，DivineBeasts.Water.SwimmingInput通过（唯一warning为既有GAS GameplayCue路径默认扫描）。隔离后仅/Temp/Untitled_1为测试脏包，真实Hub未标脏；关闭测试宿主不保存。Client/Server完整增量构建开始。资产作者PID44148重新加载磁盘新手村；本轮不再在真实资产作者中运行测试构造世界。

Task3 Ruling：本机MaterialEditingLibrary.cpp:1485/1507/1573的Scalar/Texture/Vector Setter会写入并更新实例，但bResult始终false；作者按参数名存在、真实值回读、保存和独立重载核验，不能把false当写入未发生而反复覆盖。已定位原生实现，未改引擎。母材质通过Shader编译，5采样器/估算14次像素纹理采样，不作GPU毫秒达标结论。

Task3原生保存：新手村精确水域、2500cm深度、QueryOnly水下查询和稳定MI_DBA_Village_Lake绑定已保存；湖岸/河流256点回读误差0，两个PlayerStart未变。现有WaterReview镜像水下材质与精确体积并保留Review.Lake区域ID。四份Kachujin原生泳姿序列及2D泳潜BlendSpace已保存，现有ABP在地面基础分支内按bIsSwimming切换，保留跳跃、DefaultSlot及死亡覆盖；编译0错误0警告，12玩家Profile均使用同一骨架/ABP。水下母材质的6项图结构warning全部与官方原始母材质相同，不擅自整理原始图。

Task2补充Ruling：原生APhysicsVolume仅按几何和体积优先级选区，会与已有高优先级禁泳Region及未发布/失效规则分歧。新增测试86604857构建后观察到“未发布水域仍授予资格”失败；未开始Play的瞬态World不能冒充已运行Chaos Broadphase，单元用例改为真实IsOverlapInVolume资格+PhysicsVolumeChanged通知，完整Broadphase留给实际运行。新增APhysicsVolume薄派生AGamePlatformWaterPhysicsVolume仅复用唯一WaterWorldSubsystem查询过滤原生资格，不增加移动器、Tick或复制状态；同级同世界所有权失效即拒绝。两份本轮新烘焙体积需升级此类型，再Cook。

Task3：PID45144已升级两份体积、保存并核实18,000地形采样误差0。0065e047完整Editor、2f164652完整Client/Server均通过。PID24048冷重载Village与Review的Region/体积/水下材质及四份泳姿/6采样BlendSpace，ABP为UpToDate；目标脏包0。

Task3运行：真实Standalone PIE先在浅审核池验证Native Chaos自动进入Swimming、动画一致及离水清理；固定200cm深度断言触底失败，保留RuntimeSwimming.json。探针改用实际World时间及独占输出目录，在Village调查最深点(-4500,1800)执行：下潜约660cm、平游约742cm、上浮约828cm、传送离水回Walking，五项断言通过，未调用SetMovementMode。证据RuntimeProbe-ca5cbabf与VillageSwimming.mp4；键盘焦点、联机/弱网及人工签收不由此代替。t.MaxFPS=0、VSync=0真实回读；PIE不作性能达标结论。Editor的bThrottleCPUWhenNotForeground仅本进程临时关闭以完成截图，未SaveConfig。

Final review：fresh water_final_review（gpt-6-astra）发现3项Important：重复ID被拒Actor可借同名资格；作者未拒绝普通PhysicsVolume冲突；材质作者重跑可覆盖脏包。一次修复通道，先新增失败回归，不再派发复审。材质脏包两项行为测试已观察3处失败后转2/2通过。目录职责登记随交付补齐。

Final Ruling：普通未接入合同体积采用作者期AABB保守拒绝重叠，保留有效平台Region体积的显式覆盖 — 防止原生选择绕过唯一规则 — 可能要求人工消除并无真实相交的包围盒冲突，不自动改动既有体积。

Final Ruling：审核图增加数据型Pawn/GameMode以供实际WASD/Space/Ctrl操作，直接复用主线类、同一Kachujin/ABP — 原DefaultPawn不能演示游泳 — 仅开发图，无正式身份或网络准入豁免。

Final：c21d04ca原生失败回归构建通过，PID47100观察到3项明确失败：重复ID体积借用资格、普通体积重叠未拒绝、错误后留有绑定。随后补注册成员指针检查与作者期保守重叠拒绝，材质作者已有未保存保护；真实截图另发现UE Python Rotator位置参数顺序不同于C++，审核Pawn/探针改用pitch/yaw/roll关键字，待原生回读与复采。

Final：审查Declined to judge均按原要求维持：不做氧气/溺水；仅覆盖当前12份主线Profile共用Kachujin骨架；速度/浮力及泳姿审美仍待人工；审核Pawn/GameMode须追加本轮原生编译、保存、重载及实际控制证据。Medium/High焦散仅缩强度，不声称少采样或GPU节省。

Final：c21d04ca原生失败回归构建通过，PID47100观察到3项明确失败：重复ID体积借用资格、普通体积重叠未拒绝、错误后留有绑定。随后补注册成员指针检查与作者期保守重叠拒绝，材质作者已有未保存保护；真实截图另发现UE Python Rotator位置参数顺序不同于C++，审核Pawn/探针改用pitch/yaw/roll关键字，待原生回读与复采。

Final：审查Declined to judge均按原要求维持：不做氧气/溺水；仅覆盖当前12份主线Profile共用Kachujin骨架；速度/浮力及泳姿审美仍待人工；审核Pawn/GameMode须追加本轮原生编译、保存、重载及实际控制证据。Medium/High焦散仅缩强度，不声称少采样或GPU节省。
