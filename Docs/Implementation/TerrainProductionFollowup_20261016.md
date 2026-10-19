# SSLM 地形材质续执行核验（2026-10-16）

## 本轮已经验证

- 40/40 张 Poly Haven CC0 正式 PBR 源图通过 `ValidateTerrainProductionPBR.py`，其中 Rock 法线、Scree 法线、RoadDirt 基础色仍需平铺接缝美术审核。
- `ValidateTerrainProductionIntegration.py` 通过：61 项备份、40/40 UE 纹理文件、正式十层母材质/MI、十层审核地图、4/4 PCG 权重过滤 Graph 的路径存在；这是文件/静态合同验收，非 UE 原生资源回读。
- `ValidateTerrainMaskPCGProjectionV2.py` 通过：Road（道路）与 Gameplay.Exclusion（玩法排除）独立，数据仍为校准源，不能冒充 Gaea 2.3。
- `ValidateTerrainCalibrationSource.py` 与 `ValidateTerrainGaeaProductionBoundaries.py` 通过：逐像素十层权重合计 255、校准源和现有工程数据合同满足要求。
- `ServerPresentationPolicy.py check` 成功；`TestServerPresentationPolicy.py` 32 项测试通过，但不代替实际服务器 Cook/Pak 审计。
- 原回退档案位置存在：`Game/Saved/Validation/TerrainPBR_20261012/Rollback_TerrainCalibration_SourceAndUEAssets.zip`；`RollbackRescueAudit_20261016.json` 说明旧 PNG 取自 Git 历史，旧 UE 文件取自当时干净工作目录。当前备份尚未进入持久受保护存储。

## 实际未完成、禁止虚报

1. UE5.8 独立 Editor 复验正式纹理规格、UMaterial/MI 真实 Shader 编译和材质节点合同。
2. 审核地图重载、十层 LayerInfo/权重及四图 PCG 有效采样、生成和 Bake。
3. 干燥/苔藓/湿润/轻雪/重雪/融雪六态的实际 D3D12 GPU 截图，高中低画质 GPU 指令/时间/显存实测。
4. 本次版本的 Client/Server 构建、Cook、Stage、UnrealPak 资源隔离；正式 Gaea 2.3 导出及用户最终视觉批准。

## 环境限制与后续门禁

本次连接时存在其他会话运行的 `UnrealEditor.exe`，命令行来自 `D:/UnrealEngine-5.8.0-release`，现有脚本默认 `F:/UnrealEngine-5.8.0-release` 已失效。为保护其打开的地图及资产，本轮未并发执行资源写入或独立 UE 编辑器作者命令。后续先确认编辑器独占窗口，再用正确引擎路径运行 `Tools/Unreal/Terrain/RunTerrainGaeaFullReview.ps1 -ValidateOnly -ProductionPBR`，查阅真实退出码和日志；不能把旧构建结果视作当前版本通过。

正式新手村共享 `M_DBA_Village_LandscapeBridge` 保持不变；未获生产签收前不得硬链接高精十层材质到服务器共享地图。
