# PrimitiveModel（PCG原语模型）

## 固定原语

| ID | 英文名 | 中文说明 | 1.0状态 | 典型组合 |
| --- | --- | --- | --- | --- |
| P0 | Field | 场数据，只读高度/权重/湿度等 | 必须只读 | Landscape、FieldProvider |
| P1 | Scatter | 曲面散布 | 必须 | 森林、岩石、地被、资源点 |
| P2 | Linear | 线性系统 | 必须 | 道路、围栏、栏杆、小径 |
| P3 | Connector | 连接件 | 接口/手摆 | 门、桥、打断、路口接口 |
| P4 | Parcel | 地块 | 必须 | 农田、院落、圈舍、宅基外环 |
| P5 | Assembly | 组合件 | 接口 | 建筑/岩组/桥段组合 |
| P6 | InterfaceBand | 界面带 | 必须 | 路肩、林缘、河岸、田埂 |
| P7 | Cavity | 体腔 | M4预留 | 洞穴、路堑、地基挖填 |
| P8 | SpatialGraph | 空间图 | M4预留 | 室内、地牢、下水道拓扑 |
| P9 | State | 状态 | M3仅稳定ID | 砍伐、收割、门、资源持久态 |

禁止在 1.x 增加 P10、L11 等绕过总体设计的“新原语”。Freeze/Paint/ForceSpline（冻结/刷除/强制样条）属于 Editor Override（编辑器人工覆盖），不是原语。

## 世界阶段

固定顺序：FieldRead → TerrainWrite → WaterBody → Networks → CutFillRequest → Connectors → Parcels → Enclosures → Buildings → Interiors → Scatter → GameplayAnchors → InterfaceBands → ApplyState → StreamHooks → RuntimeDetail。

1.0 禁用 TerrainWrite、CutFillRequest、Interiors；ApplyState 仅保留阶段ID；RuntimeDetail 只允许纯客户端无玩法影响内容。
