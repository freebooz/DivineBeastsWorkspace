# PriorityAndOverride（优先级与人工覆盖）

## 默认优先级

| 值 | 层 | 中文说明 |
| ---: | --- | --- |
| 100 | ManualLock | 人工冻结、地标、电影、手摆强制内容 |
| 90 | GameplayExclusion | 玩法硬排除 |
| 82 | MajorGate | 主要门/寨门 |
| 80 | Connector | 桥、连接件、路口接口 |
| 70 | MajorRoad | 主道路 |
| 68 | RoadGuard | 道路护栏 |
| 60 | WaterBody | 河湖水体 |
| 55 | YardWall | 院墙 |
| 50 | Parcel | 地块 |
| 48 | FieldFence | 田篱/圈舍围栏 |
| 40 | MinorRoad | 土路、小径 |
| 30 | Canopy | 乔木冠层 |
| 20 | Crop | 作物 |
| 18 | DecorFence | 低优先级装饰围栏 |
| 15 | RockProp | 散石/农具 |
| 10 | InterfaceBand | 路肩、河岸、林缘 |
| 5 | GroundCover | 草、落叶 |

生产真源为 `UGamePlatformPCGPriorityTableDefinition（优先级表定义）`；ini只允许提供缺省值。PriorityCarve遵循“严格高优先级切低优先级”，同优先级不互切，避免不稳定结果。

## Editor Override（编辑器人工覆盖）

Override不是P10/L11原语。当前枚举：

- Freeze（冻结）。
- PaintExclude（人工刷除）。
- ForceSpline（强制样条）。
- ManualConnector（手摆连接件）。

`AGamePlatformPCGConnectorActor（连接件放置器）`直接带OverrideMode，因此 GateOverride（强制门）不需要单独C++ Actor类。

自动推导不确定时应不生成，由人工Override补充。
