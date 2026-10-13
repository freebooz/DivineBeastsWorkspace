# 《SSLM》十份地形LayerInfo创建与验收记录

日期：2026-10-11；工程：DivineBeastsWorkspace/Game/DivineBeastsArena.uproject（UE5.8）。
归属：第三层DBAWorldPack_Village；正式资产目录：/DBAWorldPack_Village/Terrain/LayerInfos。

## 十份真实资产（名称和图层身份）

| 资产名称（UE原生.uasset） | LayerName（地形权重层名称） |
|---|---|
| LI_DBA_Terrain_Grass | LS_Grass |
| LI_DBA_Terrain_ForestSoil | LS_ForestSoil |
| LI_DBA_Terrain_Dirt | LS_Dirt |
| LI_DBA_Terrain_Rock | LS_Rock |
| LI_DBA_Terrain_Scree | LS_Scree |
| LI_DBA_Terrain_Sand | LS_Sand |
| LI_DBA_Terrain_Gravel | LS_Gravel |
| LI_DBA_Terrain_RoadDirt | LS_RoadDirt |
| LI_DBA_Terrain_FarmSoil | LS_FarmSoil |
| LI_DBA_Terrain_ShoreMud | LS_ShoreMud |

全部资产类别：ULandscapeLayerInfoObject（地形图层信息对象）。
全部混合方式：ELandscapeTargetLayerBlendMethod::FinalWeightBlending（权重混合）。

## 真实原生制作和回读

UE5.8的LayerName在Unreal Python中为只读属性；没有采用空字段占位资产。调用GamePlatformSurfaceEditor（平台表面材质编辑器模块）原生C++命令：SetLayerName、SetBlendMethod、AssetRegistry注册、SavePackage保存。修复FParse::Value默认将CSV逗号值截断的问题，显式设置bShouldStopOnSeparator=false。

由于正在运行的UE编辑器锁定原DLL，本轮使用UBT生成的响应文件及锁定MSVC将新源码编译、链接为单独后缀模块DLL，并在独立UnrealEditor-Cmd进程启动期间临时切换模块清单。每次进程退出后按原始字节恢复模块清单，并对原DLL进行SHA-256不变性校验。没有关闭、覆盖其他会话的编辑器与插件DLL。

第一次原生命令创建Grass：退出码0，记录count=1 created=1。第二次原生命令创建余下九份：退出码0，记录TERRAIN_LAYER_INFO_CONTRACT_PASS count=9 created=9。磁盘目标目录共10份真实.uasset。

随后重新启动另一UE5.8编辑器命令行进程，通过Tools/Unreal/Terrain/VerifyLandscapeLayerInfos.py（原生LayerInfo回读检查）逐项验证Package/ObjectPath、类、LayerName与FinalWeightBlending，十份全部通过；验证退出码0，最终标记TERRAIN_LAYERINFO_10_ASSETS_REAL_UE_VERIFY_PASS count=10。

## 关键日志

- Game/Saved/Logs/TerrainTenLayerInfoNative20261011.log（第1份创建）
- Game/Saved/Logs/TerrainRemainingNineLayerInfoNative20261011.log（其余9份创建）
- Game/Saved/Logs/Terrain10LayerInfoIndependentUEVerify20261011.log（全部10份独立反射回读）
- Saved/Validation/TerrainGaea/LayerInfo20261011（模块清单备份与执行输出）

## 后续边界

本工单仅创建LayerInfo并验收，**已完成**。独立L_DBA_TerrainMaskReview地图的实际R16/R8导入、四张PCG权重过滤Graph、PCG实际生成、全目标正式DLL构建与双端Cook、人工视觉验收仍属于后续P3—P7。隔离后缀DLL不等于生产常规Editor模块DLL发布；待其它构建任务释放后按常规UBT正式编译。正式Village地图和其它项目资产未更改。
