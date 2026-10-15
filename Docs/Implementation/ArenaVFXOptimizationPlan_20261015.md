# SSLM MainArena 十二生肖技能VFX高并发优化实施计划（P0—P7）

版本：V1.0｜制定日期：2026-10-15｜工程：DivineBeastsWorkspace / Game/DivineBeastsArena.uproject｜引擎：UE5.8
执行基准：AGENTS.md（全局策略1.4.4）、Docs/Architecture/SSLM十二生肖技能VFX性能与视觉质量规范.md。
**本文是实施计划而非完成证明。所有新类/资产必须在实际工程创建后再记录“已实现”。**

## 0. 边界与保护措施

- 统一GamePlatform→MobaCommon→DivineBeasts三层依赖；GamePlatformVFXClient是唯一客户端执行器，Editor只校验资产；SSLM项目层仅映射十二生肖视觉配置及项目内容。
- 不改服务器权威伤害/碰撞/控制、不新增VFX Server或平行资产管理器；Niagara预测/确认/取消沿原有RequestId/ActivationId/PredictionKey清理。
- 现有工作树含IceArrow、角色表现、Gameplay、UI等未提交修改；本任务禁止覆盖、重置和提交它们。对所有已改文件采用读取版本保护和精确增量编辑；真实UE资产通过引擎工具创建，不能制作伪uasset。
- 优先完成源码、资产配方和测试，再统一编译/真实Shader/客户端Cook/5v5压测；若编辑器/Runner或其他Job阻塞，保留实施结果并记录未验收阶段，不虚称成功。
- 面向1v1—5v5共用MainArena；Village/OpenWorld的预算保持原值，不因本轮项目竞技优化被全局压低。

## P0 现状盘点与基线（先行）

**动作**：清点Niagara System/Emitter、Effect Type、定义/画质变体、材质/Flipbook、服务器Cook剥离、HeroVFXProfile 12/12和现有预算、NDC使用。建立只读资产扫描器与JSON/Markdown报告；优先使用UE5.8 AssetRegistry/Editor工具，不能把源码搜索当二进制资产全覆盖。压测场景：10名英雄集中施法与≥150短命中事件/秒；分别记录完整Client P95/P99/GPU/CPU/真实活跃System/材质Overdraw。
**产物**：P0资产台账与性能报告；尚未可运行的现场指标记录为“未测量”。
**门禁**：明确已知/未知、当前Engine可执行路径、阻塞Job；不拿示例阈值冒充实测。

## P1 平台伸缩与Effect Type（最高优先级）

**动作**：在原GamePlatformVFX ScalabilityPolicy和WorldSubsystem链路中增加**可选场景/画质预算配置**与质量回退、预剔除；保留现有MaxActiveInstances=256和HardMaxTrackedInstances=512全局保护机制。项目层配置MainArena预算，不使平台识别生肖或MOBA身份。
分类：ArenaCritical、CombatCast、CombatProjectile、CombatImpact、CombatArea、CombatStatus、ArenaAmbient共7类中立Niagara Effect Type。真实UE Niagara Effect Type资产需通过编辑器生成并绑定Definition/Base/Platform/Quality系统。使用Max Effect Type Instances和Max System Instances层级限制以及Distance Significance/Budget Scaling。
单World候选独立/共享System合计：Low40、Medium58、High80、Epic120（非真实配置生效宣称）；关键提示有Niagara以外的保底图形。
**门禁**：1v1—5v5不受错误软限；World切换及Critical保底；现有Scalability单测通过；不同设备档位配置可测试。

## P2 共享短时事件NDC（高回报）

**动作**：中立NDC Burst/Event数据合同，优先在VFXWorldSubsystem已有去重与世界代次检查后分流；实现通用CombatImpact、GroundBurst、AmbientBurst三类共享写入。匹配UE5.8 **非Legacy** NiagaraDataChannel C++接口及对应Niagara读/条件生成模块，命中事件批量写入；不足资产不应产生永久激活空System。局部Islands分区避免全场巨型Bounds。
**禁止**：技能事实绕过Presentation直接写NDC；预测+确认两次命中爆炸；平台硬编码生肖/技能名；不带复用需求新建平行WorldSubsystem、Pool或Data Lease。
**门禁**：无数据通道/读者/资产时清晰失败并沿原有Definition回退（只对允许回退的装饰）；正确处理取消/换地图/重连/超量次要事件丢弃；GPU共享实测优于原生单事件Spawn后才能默认启用。

## P3 十二生肖技能内容资产迁移与视觉合同

**动作**：依既有HeroVFXProfile 12项视觉DNA，为每项技能形成“核心剪影不可删 + 装饰可裁剪”数据合同。首批验证火球、冰箭、奥术、燃烧：蓄力/弹道/命中/持续/地面预警分别评估Mesh + Niagara GPU + Flipbook + 材质分层；NDC处理命中碎屑、火花和烟尘，持续DoT单个附着实例。保留龙形大招/虎爪/马蹄雷等生肖独特剪影。
**门禁**：资源由UE5.8 Editor/Monolith实际创建编译保存，十二生肖/四画质台账完整；不伪造技能AbilityId或既有资产通过。

## P4 组件池、预热和空间剔除

**动作**：进入竞技场根据真实十人Loadout预载Definition/Shader，不在首发瞬间同步加载；关键弹道使用Niagara原生AutoRelease池；可选事件预裁剪/削峰，合理Fixed Bounds和离屏Visibility Culling。防止DoT每次结算生成System；避免大池闲置过量和跨World残留。
**门禁**：可复现的首发延迟、池命中率、10分钟稳定、退出再入无幽灵特效；不改真实伤害/范围/时序。

## P5 运行诊断与资产门禁

**动作**：保留stat GamePlatformVFX，增加按World/质量/Effect Type的调用、拒绝、NDC事件吞吐、Pool命中、首次激活和独立/共享System预算统计；Editor检查Effect Type匹配、FixedBounds/LOD、未绑定画质档、可疑过绘/过多Emitter等。静态测试和UE Automation Testing覆盖边界与降级。
**门禁**：运行时真实Niagara系统数与追踪请求数分开报告；CI中红线/黄线有中文诊断原因。

## P6 完整构建与GPU压力验收

**动作**：完成所有源码及资产修改后统一执行已有UE5.8 Editor/Client/Server构建、原生Niagara GPU Shader检查、Client Cook/Stage、Server零VFX资产审计；以打包客户端在多档机器、不同画质分别测量1v1—5v5。
**候选门禁（非保证）**：High 60FPS，P95≤16.67ms、P99≤20ms、VFX GPU≤1.5ms、VFX CPU≤0.6ms；Epic 120FPS按8.33ms单独验证。实测硬件/分辨率/驱动/功耗档必须记录。
**阻断**：其他项目Job占用编译、UE编辑器未连接、GPU采样和真人/机器人5v5流程不可用时须报告真实未验收项，不凭静态检查伪称通过。

## P7 人工视觉验收与交付

**动作**：在插件演示场景及MainArena逐项核验十二生肖技法蓄力、释放、飞行、撞击、持续和消散；Low保持危险预警与核心剪影，Epic保留神秘、魔幻、电影质感和打击满足感；对比前后渲染录像/截图和帧数据。
**交付**：完整变更清单、版本记录、测试日志、资产索引、问题清单、待补/风险及结论。重要视觉要人工确认，不能用“已建.uasset”代替。

## 实施策略与版本兼容

优先修改无冲突的中立源文件、文档、自动检查脚本；对于已有未提交修改以变更快照隔离；UE5.8 Niagara NDC API使用当前引擎真实非旧版接口（不得引用已弃用Legacy写入接口）；在蓝图和C++入口均使用中文语义文档说明。P0/P1/P2可以分阶段交付，但每阶段必须独立核对证据。
