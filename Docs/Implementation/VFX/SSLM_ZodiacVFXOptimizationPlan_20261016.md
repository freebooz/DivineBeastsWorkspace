# SSLM 十二生肖技能 VFX 优化实施计划

版本：V1.0｜2026-10-16  
范围：MainArena（统一竞技场）1v1/2v2/3v3/4v4/5v5  
目标：在高并发技能释放下优先保持技能轮廓、命中瞬间、危险区域和打击反馈，同时控制 Niagara（特效系统）实例、CPU/GPU模拟、透明过绘和资源加载开销。

## 一、执行原则
1. Gameplay（玩法）权威逻辑仍由Dedicated Server（专用服务器）/GAS承担；VFX只表现事实，不参与伤害、范围、控制和比赛结果。
2. 保持GamePlatform（平台层）→MobaCommon（MOBA通用层）→DivineBeasts（SSLM项目层）三层依赖方向。平台层只提供通用播放、预算、共享事件、池化和诊断；生肖视觉由项目内容包管理。
3. 优先减少独立Niagara System（系统）数量和高成本透明层，而不是粗暴减少所有粒子。关键技能必须有低成本保底表现。
4. NDC（Niagara Data Channels，Niagara数据通道）仅用于高频、短生命周期、非权威的共享表现；独立弹道、持续状态、可取消前摇和大招主体继续使用现有VFX执行器。
5. Flipbook（序列帧）适用于火焰、烟雾、爆炸等形态复杂且生命周期短的表现，但必须以真实过绘、显存和视觉效果测量决定尺寸与帧数。

## 二、实施阶段
### P0：真实基线与资产盘点
盘点Effect Type、Scalability、Definition、Niagara System/Emitter、材质、对象池、NDC桥接、英雄视觉配置；统计MainArena实际入口及其他Niagara；输出System/Emitter/粒子峰值、VFX CPU/GPU、透明过绘、首次施法卡顿和内存。严格区分代码存在、原生UE资产存在和实际运行通过。

### P1：MainArena专用预算
开启现有WorldBudgetRule，仅匹配Experience.MainArena.Main。Low/Medium/High/Epic采用40/58/80/120活跃System候选评审值，不作为引擎硬上限；Critical通过现有关键路径和非Niagara保底；不影响OpenWorld/Village。

### P2：高频共享事件
验证UE5.8原生Data Channel Asset和GPU Reader；优先完成CombatImpact和GroundBurst，AmbientBurst最后实施。共享事件必须经过请求去重、世界代次、资源租约和取消检查；通道未通过真实Editor验证前保持默认关闭；比较单事件Spawn与共享写入CPU开销。

### P3：十二生肖技能分层
普通技能采用核心System+共享命中；持续状态单System持续更新；地面区域用Mesh/Decal/材质保证准确轮廓；终极技能允许多Emitter但必须通过10人集中释放测试。逐英雄保留既有视觉DNA：子鼠弧刃/残影、丑牛重压/震裂、寅虎爪痕/爆发、卯兔玉环/花瓣、辰龙云气/龙形、巳蛇S形鳞光、午马雷电脉络/蹄印、未羊玉角/护盾、申猴棍弧/残影、酉鸡金羽/鸣波、戌狗追猎印记、亥猪重压波/玄鬃。

### P4：资源与材质优化
检查透明材质Overdraw、SceneColor、折射、动态光源和阴影；火焰/烟雾/爆炸优先合理尺寸Flipbook，核心主体优先Mesh/材质；大量纯视觉粒子优先GPU Simulation，CPU Simulation仅用于必要交互；使用准确Fixed Bounds；距离LOD优先裁减烟、碎屑、余烬和热浪。

### P5：生命周期与池化
复用现有Niagara Pool，禁止第二套播放器/池真源。Spawn前检查距离、视锥、预算和资源状态；预测/确认/纠正/取消不得重复爆炸；World退出、换场、重连、死亡、结算清理实例、Timer、异步回调、NDC监听和资源租约。

### P6：自动检查
完善Definition Validator和Automation Test，检查Effect Type、Quality Variants、Fixed Bounds、NDC配置、服务器隔离、生命周期和重复请求，并增加Arena配置一致性检查。

### P7：统一编译与性能验证
完成代码、配置、蓝图、Niagara和材质后统一编译；Editor/Client/Server分别验证，Dedicated Server不得携带纯客户端VFX。5v5最坏场景覆盖10人集中释放、多个AOE、连续命中、150次/秒短事件压力。记录P95/P99帧时间、VFX CPU/GPU、System峰值、Overdraw、内存和首次施法卡顿。High候选目标：60FPS、VFX GPU≤1.5ms、CPU≤0.6ms；最终以真实硬件/分辨率为准。

### P8：人工验收与归档
逐技能检查蓄力、释放、飞行、命中、持续、结束；检查近景电影感、中景可读性、远景轮廓、敌我区分和高并发视觉稳定性。无真实UE运行证据不得标记Production Ready（生产就绪）。

## 三、当前实施边界
WorldBudgetRule代码已实现但默认关闭；NDC写入桥代码已存在，但CombatImpact/GroundBurst原生通道和GPU Reader仍标记未验证；Effect Type和质量变体验证骨架、对象池、生命周期、Definition Lease和去重机制已存在。因此不重复建设这些机制，以“配置落地→真实UE资产→十二生肖接入→统一编译→压力测试→人工验收”为主线。

由于当前工作区仍有7个活跃任务占用执行资源，本次不能强行覆盖其正在修改的文件，也不能伪造UE原生.uasset或GPU性能数据。可安全实施的配置和计划文档先落地，其余按P0—P8继续执行。
