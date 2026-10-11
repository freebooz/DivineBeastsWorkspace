# 蝴蝶与鱼群纯行为机制实施报告（2026-10-11）

## 范围与归属

本子任务在正式工作空间的 GamePlatformVFXClient（平台 VFX 客户端模块）内部实现群落行为，供既有世界 VFX 实例调用。没有新建插件、Subsystem（子系统）、播放器、Actor（场景对象）、资产加载器或预算系统；源码不依赖 UE 类型、Niagara、MOBA 或项目资产。独立原生测试直接编译同一份正式生产 .cpp，没有复制算法或伪造引擎头文件。

本次只维护五个文件：

- Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Environment/Fauna/GamePlatformFaunaSimulation.h：内部行为合同、只读输出、实例所有权及参数中文说明。
- 同目录 GamePlatformFaunaSimulation.cpp：蝴蝶状态机、花点预约、鱼群网格邻域、几何扫掠及固定步。
- Tests/VFX/Fauna/FaunaSimulationTests.cpp：实际生产源的原生回归。
- Tests/VFX/Fauna/RunFaunaSimulationTests.ps1：Windows Visual C++ 发现、编译、运行与证据保存入口。
- 本报告。

执行基线为 main 分支 bd106b3259c1a1917c0ece46d3ec2a8edd227fd4。其他任务已有修改保持不动；没有提交、推送、启动 UE 或 Blender。总体目录规划由主任务统一登记。

## 实际行为

蝴蝶具有 Flying（游飞）、Approaching（接近）、Perched（停栖）、TakingOff（起飞）、Fleeing（受惊）五态。选择合法空闲花槽后立即预约；减速接近，真正到达后将速度归零，并沿归一化花面法线停栖。停留结束或受惊释放槽位，接近超时也释放。容量为零的花点不接受预约；无花点或无空槽时持续游飞。

容量大于一的花点在切平面形成中心槽位网格，间距为 2.2 倍个体约束半径。槽位若落入障碍或边界外则不接受预约。此球半径用于中心几何约束，不代表完整翅膀网格碰撞体。

鱼群具有 Swimming（游动）与 Fleeing（受惊）状态。每固定步先按位置建立空间网格，从同一旧快照读取邻居。分离、速度对齐、局部中心聚合、巡游目标和边界排斥共同影响速度，最后统一交换输出缓冲。测试已证实只改变邻居数量上限、保持出生布局不变，会改变实际速度，行为不是仅生成统计数据或独立解析轨迹。

每体每固定步最多接受 MaxNeighbors 个邻居，最多检查 max(32, 4 * MaxNeighbors) 个候选，最多访问 27 个相邻网格单元。候选上限包括被距离过滤的个体，密集桶内采用旋转访问起点。此为算法工作量边界，不是 CPU/GPU 帧预算或实机性能承诺。

所有个体中心保持在扣除 BodyRadius（个体半径）的盒体内。鱼额外保持在简单 XY 水域多边形内并与岸线保持半径距离；球障碍采用扩张半径约束。移动检查连续线段，避免高速时合法终点掩盖中途穿岛或越过凹水域缺口；阻挡时尝试合法切向分量，再求合法移动前缀。

## 精确接入合同

命名空间为 GamePlatformFauna，类型为 FGamePlatformFaunaSimulation。既定六个入口签名保持不变：

~~~cpp
bool Initialize(const FSettings&,
    const std::vector<FFlower>&,
    const std::vector<FObstacle>&,
    const std::vector<FVector>& WaterPolygon,
    std::string& OutError);

void Step(double DeltaSeconds);

bool SetDisturbance(const FVector& Center,
    double Radius, double Strength, double DurationSeconds,
    std::string& OutError);

void Reset();
const std::vector<FAgent>& GetAgents() const;
const FStatistics& GetStatistics() const;
~~~

FSettings 未增加字段。Count 为 int，合法 0..512；Seed 为 std::uint32_t；MaxNeighbors 为 int，合法 1..32。位置是相对栖息区中心的局部厘米坐标，速度为厘米/秒，时间为秒。FixedStepSeconds 默认 1/30 秒、合法 1e-5..0.25 秒；每次 Step 最多补算八步，额外时间计入丢弃统计，不转换成巨大位移。

输入数值设置了防溢出的实现上限：半范围每轴不超过 1e8 厘米且大于 BodyRadius 加 1e-5 厘米；BodyRadius 不小于 1e-5 厘米；速度不超过 1e7 厘米/秒；邻域半径不超过 1e8 厘米，分离半径不大于邻域半径。花点最多 256 个、每点容量 0..512、总槽位不超过 4096；障碍最多 256 个；水域顶点最多 128 个。多边形必须简单、非退化、顶点位于栖息盒 XY 范围；Z 不参与多边形形状，但输入各分量仍需有限。详细数值与单位以生产头文件及校验函数为准。

FAgent 额外增加一项输出：FVector UpVector（单位上向量）。停栖时为花面法线，其他状态为局部 Z 上。适配层可据速度和此向量生成实例朝向。AnimationPhase 为 [0,1) 周期相位，FlightWeight 为 [0,1] 姿态权重；FlowerIndex=-1 表示没有预约。

FStatistics 的精确字段为：

- std::uint64_t NeighborChecks：最近一次有效 Step 全部子步实际检查候选总数。
- std::uint64_t NeighborLinks：同一次 Step 接受的邻居总数。
- std::uint32_t OccupiedFlowerSlots：当前预约总数，包括接近与停栖。
- std::uint32_t FixedSteps：最近一次有效 Step 的实际子步数，0..8。
- std::uint64_t TotalFixedSteps：Initialize/Reset 后累计固定步数。
- double DroppedSeconds：因子步上限丢弃的累计秒数，饱和处理防诊断值溢出。

SetDisturbance 的中心是局部坐标；半径为正且不超过 1e8 厘米，强度为 0..1，持续时间为正且不超过 3600 秒。强度零清除当前刺激；非零有效调用立即将范围内个体转为受惊并释放蝴蝶预约。非法调用保留旧刺激和群落。当前合同只有一个可替换局部刺激，没有多个并发刺激列表。

Initialize 在完整候选状态通过校验、容量分配和合法出生采样后才替换旧状态。失败返回 false 与中文错误，并保留旧个体、预约、刺激和随机序列。出生位置采用有界拒绝采样和有界网格兜底；失败文案明确是预算内未找到位置，不声称数学无解。运行中游走目标采样失败回退至合法的当前中心。

方法由单一拥有者线程调用，没有跨线程锁。GetAgents 引用有效期截止下一次非 const 操作，装配层须及时转换到 Niagara 数据接口。世界退出、实例取消和终止清理应 Reset 或销毁实例；这些 UE 生命周期动作由主任务接入。

## 测试驱动与实际结果

先创建测试及编译入口，在正式头文件和 .cpp 尚未存在时实际运行：

~~~powershell
pwsh -NoProfile -File Tests/VFX/Fauna/RunFaunaSimulationTests.ps1 -RunId Red_MissingImplementation_20261011
~~~

该命令真实调用 Visual C++，因正式生产头/源缺失返回编译退出码 2。证据位于 Saved/Validation/FaunaNative/Red_MissingImplementation_20261011/。它证明失败入口生效，不代表行为用例已经执行。

第一版以 Implemented_20261011_01 运行，编译和 12 个用例通过。随后自审发现固定绝对距离容差会使极小球障碍失效，新增覆盖全栖息盒的微小球体回归，先真实运行得到 14 通过、1 失败、退出码 1，证据位于 Saved/Validation/FaunaNative/Red_TinyGeometry_20261011/。

修复采用随半径平方缩放的相对容差，盒边界容差限制为浮点舍入量；同时将集合大小检查前移至复制前，补齐合法游走目标回退。最终实际命令：

~~~powershell
pwsh -NoProfile -File Tests/VFX/Fauna/RunFaunaSimulationTests.ps1 -RunId Verified_20261011_02
~~~

实际工具链为 Visual Studio 2022 BuildTools 的 x64 Visual C++，工具集目录版本 14.44.35207；参数含 /std:c++17 /EHsc /utf-8 /W4 /WX /O2。完成时间 2026-10-11T13:31:30.5918845Z。编译退出码 0，测试退出码 0；结果为 **15 passed, 0 failed**。

真实 .obj、.exe、Compile.log、Tests.log、Result.json 位于 Saved/Validation/FaunaNative/Verified_20261011_02/，已回读文件存在和退出码。最终通过范围：

1. 蝶/鱼分别验证 0、1、128、512 个体，数量不漂移、位置/速度/动画有限。
2. 非法数量、NaN 范围、无限速度、过量邻居、零步长、坏花法线、负障碍半径原子失败。
3. 同种子与等价固定步分块重现。
4. 无花点持续游飞。
5. 单容量预约、真实停栖、释放起飞。
6. 蝴蝶刺激立即释放及刺激消失后恢复。
7. 512 鱼真实邻域查询的候选检查上限。
8. 128 鱼在 L 形凹水域和球形岛障碍内持续运行。
9. 15000 厘米/秒下连续移动路径不穿过球障碍。
10. 自交多边形与全覆盖障碍失败且保留旧群落。
11. 非法时间无副作用，非法刺激不覆盖有效刺激。
12. 128 鱼运行 18000 个 1/30 秒固定步；刺激恢复、有限性、超长帧上限及 Reset。
13. 极小尺度几何也拒绝全覆盖障碍。
14. 邻居上限改变实际转向，并存在真实邻居链接。
15. 禁用花点、倾斜法线停栖与已占花点 Reset 释放。

上述 18000 步等于 10 分钟模拟时间，不是等待十分钟的实时整机稳定性测试。

## 自审与材料身份

中文说明与实现自审覆盖输入原子性、随机流所有权、花点预约释放、双缓冲邻域、候选数量界限、移动扫掠、尺度容差、子步上限与重置。原生警告作为错误编译；没有以空实现、固定成功或伪造统计替代行为。

最终文件 SHA-256：

- 生产头：D8B06401AEB10FDEC824662E411D5CDDDD11DABAC44CCF93D4859E4E71B50E51
- 生产源：5AEBA68387F54C5746042465FEA1FA3E8E04EBD2852EFD95D244EE888E565B10
- 原生测试：A57208AD9B676E3E0211427A9855F6D6B41947C102ED06A994437CE1FEC681A7
- 测试入口：8245B393AF888A0127A5896FE3B092806FA38026095300179BE9390EDACCC322

## 未由本子任务证明的部分

本报告只交付并验证 CPU 行为机制。Niagara 数组传输、朝向、模型/材质、花点与水域场景编辑、质量档、既有 VFX 租约/取消清理、UE 编译、GPU 渲染、Cook、服务器资产剥离和人工美术审核由主任务继续验证。当前不是 GPU 邻域计算；没有 GPU、移动设备或整机帧时数据，不据此承诺并发帧率。

水域为静态简单 XY 多边形加盒体深度，障碍是静态球体集合，没有动态水深、河流流速场、任意网格距离场或带洞多边形。蝴蝶不使用障碍全局寻路；不能直达的花槽预约会在接近超时后释放。鱼分离力是群游行为，不是逐个完整网格的刚体互碰撞求解。

相同种子与同一编译器运行可重现测试结果；未宣称跨平台浮点位级确定性或服务器权威一致性。
