# 通用环境表面制作工具

本目录用于平台层苔藓、积雪和湿润相关源纹理与真实虚幻编辑器资产制作脚本，不承担网络天气权威。

## 通用材质函数首批UE编辑器作者脚本（2026-10-13）

`AuthorMaterialCoreFunctionsUE58.py`（真实UE5.8五函数作者）包含高度混合、宏微观混合、地形生物群落遮罩、距离细节与结霜PBR；默认`--mode inspect`（仅规划），必须在确认的正式UE编辑器内显式选择`--mode apply`（真实创建）或`--mode verify`（已保存资产只读验证）。已存在资产保持原状，绝不为了重排函数而覆盖旧包。`Tools/Unreal/VFX/AuthorVFXMaterialFunctionsUE58.py`（平台VFX三函数作者）采用相同原则；`Tests/Assets/TestMaterialFunctionAuthoringScripts.py`（离线节点拓扑测试）不是UE Shader验收。完整身份、三层归属与实际阻断见`Docs/Implementation/Materials/MaterialAuthoringBatch_20261013.md`（首批八函数实施记录）。


## 苔藓—积雪PBR材质制作流程

1. 运行 `python Tools/Unreal/Surface/GenerateMossPBRSourceArt.py --create` 生成五张可平铺源图（平台SourceArt）；后续运行 `--verify` 只读验收源图哈希、尺寸和拼接边界。
2. 默认运行 `python Tools/Unreal/Surface/AuthorMossSnowPBRAssets.py` 仅显示预定UE资产地址，不生成资产。
3. 在正式UE5.8编辑器的Python脚本环境中设置 `MOSS_SURFACE_MODE=apply`，执行同一脚本创建真实Texture2D（引擎纹理）、`M_GP_Surface_SnowMoss_PBR`（平台母材质）和 `MI_DBA_MossSnow_Cliff`（项目表现材质实例）。该脚本不修改原有雪母材质、正式Village地图或DedicatedServer（专用服务器）。
4. 设置 `MOSS_SURFACE_MODE=verify` 再在引擎中运行，验证真实资源类型、母材质连接数量和实例父级；运行 `python Tests/Assets/ValidateMossSnowCoverageResponse.py` 检查积雪与苔藓覆盖曲线。

**验收**：源图、脚本与引擎材质是三个不同门禁；Shader（着色器）、Client Cook（客户端资源烘焙）、Server Cook（专用服务器资源排除）、PIE（编辑器运行画面）与人工质量签收均需分别提供实际证据。
