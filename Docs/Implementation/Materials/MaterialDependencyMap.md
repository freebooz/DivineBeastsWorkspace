# 通用材质函数库：三层与领域插件依赖图（静态合同）

```text
DivineBeasts（神兽联盟项目层）
  ├─ DBAWorlds / DBAWorldPack_*（世界逻辑与地形专属材质内容）
  ├─ DBAClient / DBAPresentationPack_Core（十二生肖/VFX项目实例）
  ├─ DBAUIPack_Core（项目界面主题与材质实例）
  └─ DBAArena（项目竞技组合）
           │ 仅按真实使用者组合
           v
MobaCommon（可选MOBA通用表现）
  └─ MobaPresentation（竞技可共享的范围/阵营/技能表现语义）
           │
           v
GamePlatform（游戏平台通用层）
  ├─ GamePlatformSurface（独立环境着色/SurfaceMark/8参数MPC）
  │    └─ GamePlatformData/Core（共享数据与平台核心，仅必需依赖）
  ├─ GamePlatformVFX（Niagara外观材质/通用VFX执行）
  ├─ GamePlatformUI（UMG/CommonUI样式/界面材质域）
  ├─ GamePlatformWeather（天气事实；Client可调用Surface）
  ├─ GamePlatformPCG（生成/选择，调用方配置项目MI）
  └─ GamePlatformPresentation（中立语义，不直接覆盖Material）
```

**重要：** 图按“所有权/使用方向”展示，不代表VFX、UI与PCG必须相互依赖。Surface不得反向引用天气、世界、PCG、VFX、UI或第三层资源。服务端天气与碰撞/GAS权威数据不得依赖客户端材质域。

## 现有冻结路径

- `/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal`（八环境参数集合）
- `/GamePlatformSurface/Materials/M_GP_Surface_Master`（完整母版）
- `/GamePlatformSurface/Materials/M_GP_Surface_Lite`（轻量母版）
- `/GamePlatformSurface/MaterialFunctions/Masks/`（坡度、高度遮罩）
- `/GamePlatformSurface/MaterialFunctions/Utility/`（世界噪声）
- `/GamePlatformSurface/MaterialFunctions/Layers/`（雪、苔藓、湿润、积水）

具体七个函数唯一路径参见`MaterialAssetInventory.json`（冻结合同）。上述静态路径本身不能证明材质文件真实存在。
