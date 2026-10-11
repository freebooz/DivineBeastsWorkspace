# 任务2：通用蝴蝶与鱼群生产算法
## 所属与文件所有权
仅创建/修改：
- Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Environment/Fauna/GamePlatformFaunaSimulation.h
- 同目录GamePlatformFaunaSimulation.cpp
- Tests/VFX/Fauna/FaunaSimulationTests.cpp
- Tests/VFX/Fauna/RunFaunaSimulationTests.ps1
- Docs/Implementation/FaunaSimulationReport_20261011.md
不修改其他VFX/项目文件；不运行UE/Blender，不提交，不派子代理。

## 精确合同
生产代码为标准C++17，无UE/网络/渲染/资源加载依赖。命名空间GamePlatformFauna；类FGamePlatformFaunaSimulation。
定义FVector(double X/Y/Z)、EKind(Butterfly/Fish)、EBehavior(Flying/Approaching/Perched/TakingOff/Fleeing/Swimming)、FSettings、FFlower、FObstacle、FAgent、FStatistics。
FSettings：Kind、Count、Seed、HalfExtent、CruiseSpeed、BodyRadius、NeighborRadius、SeparationRadius、MaxNeighbors、FixedStepSeconds。Count合法0..512；MaxNeighbors合法1..32；步长默认1/30秒；位置是栖息区中心局部厘米坐标，速度cm/s；半范围/速度等正且有限。
FFlower：Position、Normal、Capacity。FObstacle：Center/Radius球体。WaterPolygon是vector<FVector>的简单XY多边形，空时用HalfExtent盒；Z范围结合BodyRadius。
FAgent：Position、Velocity、AnimationPhase、FlightWeight、Behavior、FlowerIndex；可增必要内部状态。
Initialize(const FSettings&,const std::vector<FFlower>&,const std::vector<FObstacle>&,const std::vector<FVector>& WaterPolygon,std::string& OutError)->bool。
Step(double DeltaSeconds)->void。
SetDisturbance(const FVector& Center,double Radius,double Strength,double DurationSeconds,std::string& OutError)->bool。
Reset()->void；GetAgents()->const std::vector<FAgent>&；GetStatistics()->const FStatistics&。
如增字段/方法报告精确合同；不要更名已定方法。

## 行为
蝴蝶盒内游飞、择花、减速接近、按法线停栖、停留后起飞。花点有预约/释放，受惊释放，无容量仍游飞；空花点可运行。
鱼采用空间网格有限邻域(分离/对齐/聚合/巡游)与双缓冲更新，每体查询数量有上限；保持盒体/多边形内部、避球障碍，受惊恢复。不得以独立解析轨迹冒充Boids。
初始化寻找合法位置，无有效空间明确失败。固定步长与子步上限，长稳有限坐标；所有非法输入失败保留既有合法状态。Count0合法且空；Reset清占位/刺激。
Statistics至少给邻居检查数、占位数量，供行为断言；不伪造GPU性能。

## 真实测试
测试链接实际生产cpp，不复制算法、不伪造UE头。先写断言并记录缺实现/失败，再实现并运行。
只读发现Windows C++工具链cl/cmake；入口从自身路径解析工程，临时构建到Saved/Validation/FaunaNative，传播失败码。
至少覆盖非法/非有限且不污染、种子复现、0/1/128/512、空花点/单容量占位/停栖释放/受惊恢复、鱼边界/岛障碍/邻居有界、长时间位置有限/Reset。
报告写实际命令/结果/限制；UE编译与图形验收由主代理负责。
