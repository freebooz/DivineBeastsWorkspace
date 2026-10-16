# GamePlatformVFX 竞技场共享短时特效接入规范（UE5.8）

> 所属模块：GamePlatformVFXClient（跨游戏客户端VFX机制）；应用项目：SSLM MainArena。
> 状态：**源码原型已接入，Niagara Effect Type/NDC原生资产及GPU读者未生成/未验收**。默认未启用NDC，不得作为正式已优化的性能依据。

## 1. 当前代码链

既有Presentation请求进入UGamePlatformVFXWorldSubsystem（世界服务）进行RequestId/PredictionKey幂等与World生命周期校验，复用GamePlatformData共享Definition/选中资源Lease（租约）。UGamePlatformVFXDefinition（特效定义）新增可选SharedBurstDataChannel（NDC软资源）和bUseSharedBurstChannel（开启标识，默认false）。

只有Instant（一次性命中）的Definition可启用。若NDC Asset已就绪且UE5.8写入器有效，FGamePlatformVFXDataChannelBridge（共享瞬时事件桥）写入一条GPU可读记录，现有实例句柄立即按终态清理并保持请求去重；不创建单事件Niagara System。若桥接返回失败，使用既有Niagara原生Spawn/Pool路径。缺少NDC资源的预载申请失败要走项目的定义/资源失败路径，**不能把缺失通道宣称为总可回退成功**；项目进入启用状态前须验收资产齐全性。

当前仅有单条写入API。高并发优化后续必须比较Writer/事件与批量Writer/帧的CPU开销，防止每次写入的分配吞掉共享收益。当前Niagara Editor和独立GPU读者System均未执行制作，默认开关保持false。

**UE5.8专项兼容风险：** Epic官方问题跟踪 UE-399252 披露：如果在引擎早期初始化阶段即使将 `fx.Niagara.DataChannels.Enabled`（UE5.8 NDC开关）重复设置为当前默认true，也可能触发Niagara数据接口注册时序断言；当前官方标注影响5.8、目标修复6.0。SSLM不得为了启用本方案在Default*.ini中重复写该开关或借此修改Niagara插件源码。默认NDC功能是否已启用需在真实编辑器启动后只读检查，原生资产生产及运行需单独回归。参考：https://issues.unrealengine.com/issue/UE-399252

## 2. 必须使用UE5.8原生资产制作的内容

建议真实资源落在：
- `/GamePlatformVFX/Arena/EffectTypes/NET_GP_*`（七类中立Effect Type资产）。
- `/GamePlatformVFX/Arena/DataChannels/NDC_GP_*`（CombatImpact、GroundBurst、AmbientBurst三类原生数据通道）。
- `/GamePlatformVFX/Arena/Niagara/NS_GP_Arena*`（共享GPU读者/有限生命周期系统）。

完整候选清单由 `Tools/Unreal/VFX/Arena/ArenaVFXAssetsSpec_V1.json`（JSON制作契约）维护，审计 `AuditArenaVFXAssets.py` 仅检测文件名和规格，**不等于实际AssetRegistry**。只有通过Unreal Editor/Monolith真实创建、编译、保存、独立重载后才能标记有效。

每份NDC包含六个具名字段：
- `Position`（Position/高精度世界坐标，cm）。
- `Normal`（Vector3/接触表面法线）。
- `Color`（LinearColor/线性颜色）。
- `Scale`（Float/视觉尺寸倍率）。
- `StyleId`（Int/通用表现样式序号）。
- `Seed`（Int/本次事件随机种子）。

上述是当前通用桥接写入字段合同，真实Editor资产必须核对**名称与实际数据类型**，不能只检查至少有六个变量。CPU请求身份和取消/代次留在已有WorldSubsystem，不能把可复用FGuid或Gameplay信息当作GPU直接消费字段。

Niagara读者按照类别用与通道一致的空间Context/Islands读取同帧或上一帧数据；前者有写后读顺序依赖，后者可降低同步耦合但会增加1帧延迟。竞技关键打击瞬间应测实际相位一致性，不能把延迟命中错当无影响。独立Reader固定边界不得覆盖整个竞技场，合理设置局部区域、活跃时间和系统空闲回收。

## 3. 画质和预算

平台WorldBudgetRule（世界预算）仅是请求追踪软限，不是Niagara System数或GPU耗时；默认关闭。每个Effect Type按Low/Medium/High/Epic分别配置原生Max Effect Type Instances、Max System Instances和重要性/距离剔除；Critical的AOE精确轮廓由可读的Mesh/Decal/材质保底。

MainArena首次性能验证目标按单客户端World：Low 40、Medium 58、High 80、Epic 120 活跃独立/共享System候选，必须包含其他非GamePlatformVFX入口系统。High 60FPS暂用GPU VFX 1.5ms、CPU VFX 0.6ms作候选测试目标；目标不是已测事实和跨设备保证。

## 4. 验收

1. 用真实UE5.8 Editor检查Definition的所有QualityVariants与Effect Type/FixedBounds匹配，检查NDC字段名称与类型、GPU读者渲染及材质。
2. 单次命中时NDC事件存在、普通Niagara回退可信、预测/确认不重复爆炸；角色换代、World退出无幽灵事件。
3. 1v1—5v5压力，High ≥150次/秒短时命中模拟与多个大招/持续AOE一起出现，比较单事件Spawn与共享写入的GPU/CPU实测和P95/P99帧时间。
4. 新增stat GamePlatformVFX的Shared Burst Writes（共享命中写入）与Shared Burst Fallbacks（通道回退）只是内部事件计数；不能替代Niagara活跃系统统计。
5. Editor/Client/Server完整编译、Niagara GPU/Shader、客户端Cook/专服零纯表现资源、人工视觉验收全部完成才能打开正式默认开关。

## 5. V2.0桥接与诊断增量（2026-10-16）

- `WriteInstantBurst`（单次写入）仍为既有同步执行链，增加命中位置上下文；`WriteInstantBursts`（最多128次）是供未来已去重、同通道、同空间策略的批处理API。多事件仅支持Global，不能误把远距离不同Islands写入同一Writer。
- 逐事件调用对Niagara空间上下文提供位置；当前本地UE5.8中部分数据通道仍继承Legacy上下文，缺少可验证的位置输入时直接返回false并沿旧路径回退，不会假称Islands已投入使用。
- `gp.VFX.SnapshotComponents`提供手动World近似组件统计；结合真实Niagara与GPU Profiler检查，不能与平台请求计数混用。
- 专项离线规格增加请求/真实System/CPU-GPU三个维度与未验收标志；测试运行不等于任何Niagara原生资产已生成，也不等于Writer写入一定被Reader绘制。
- 当前生产限制不变：尚无原生7类NET、3类NDC和GPU Reader绑定，不启用`bUseSharedBurstChannel`。将来开启前需验证UE5.8具体版本的通道类型、读写Tick顺序、同/上一帧读取、Islands路由、Reader空闲清理、预测纠正和统一Cook。

