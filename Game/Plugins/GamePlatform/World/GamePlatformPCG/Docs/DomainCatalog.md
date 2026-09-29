# DomainCatalog（PCG领域语义目录）

领域 ID 用于“原语 + Definition（定义）+ Template（模板）”组合，不代表每项创建一个 C++ 类。

| 领域ID | 中文说明 | 原语/机制 | 计划阶段 |
| --- | --- | --- | --- |
| Field.Height | 地形高度场 | P0 | M0 |
| Field.BiomeWeight | 群系权重场 | P0 | M1接口 |
| Field.Buildable | 可建/可耕掩码 | P0 | M1接口 |
| Forest.Canopy | 乔木冠层 | P1 | M0/M1 |
| Forest.Understory | 林下灌木幼树 | P1 | M1 |
| Forest.Floor | 草/落叶地被 | P1 | M1 |
| Forest.Edge | 林缘 | P6 | M2接口 |
| Rock.Scatter | 散石 | P1 | M1 |
| Rock.Formation | 岩组组合件 | P5 | M2接口 |
| Agri.Parcel | 农业地块 | P4 | M1 |
| Agri.Crop | 垄作作物 | P4+P1 | M1 |
| Agri.Orchard | 果园 | P4 | M2接口 |
| Agri.Fallow | 休耕地 | P1 | M2接口 |
| Road.Network | 道路网络语义 | P2 | M1 |
| Road.Surface | 道路表面 | P2 | M1 |
| Road.Shoulder | 路肩 | P6 | M1 |
| Path.Trail | 林径/小径 | P2 | M1 |
| Water.River | 河流 | P2 | M2接口 |
| Water.Lake | 湖泊 | P4 | M2接口 |
| Water.Bank | 河岸/湖岸带 | P6 | M2 |
| Bridge.Span | 桥跨语义 | P3+P2 | M1手摆/M4自动 |
| Gate.Farm | 农门 | P3 | M1 |
| Break.Road | 道路打断围合 | P3 | M1 |
| Encl.FieldFence | 田篱 | P2 | M1 |
| Encl.Paddock | 圈舍围栏 | P2 | M2接口 |
| Encl.YardWall | 院墙/院篱 | P2 | M2接口 |
| Encl.HedgeRow | 绿篱 | P2/P6 | M2接口 |
| Encl.RoadGuard | 道路护栏 | P2 | M2 |
| Encl.CliffRail | 临崖栏杆 | P2 | M2 |
| Encl.DockRail | 码头栏杆 | P2 | M2 |
| Settle.Parcel | 聚落宅基地块 | P4 | M2接口 |
| Settle.Building | 聚落建筑 | P5 | M2接口 |
| Settle.Yard | 聚落院落 | P4+P5 | M2接口 |
| Play.Resource | 资源锚点 | P1 | M1 |
| Play.Cover | 掩体锚点 | P1 | M2接口 |
| Play.Climb | 攀爬锚点 | P1 | M2接口 |
| Play.Spawn | 刷新/出生锚点 | P1 | M2接口 |
| Gameplay.Exclusion | 玩法硬排除 | Exclusion | M0/M1 |
| State.Harvest | 收割状态语义 | P9 | M3 |
| State.Chop | 砍伐状态语义 | P9 | M3 |
| State.Gate | 门状态语义 | P9 | M3 |
| State.Persist | 持久化状态语义 | P9 | M3 |

项目不得向本目录加入 `DivineBeasts.Village.Peach` 等项目专属 ID；项目内容应使用上述通用语义并通过 Data Asset（数据资产）提供具体资源。
