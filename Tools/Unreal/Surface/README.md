# 通用环境表面制作工具

本目录用于平台层苔藓、积雪和湿润相关源纹理与真实虚幻编辑器资产制作脚本，不承担网络天气权威。

## 苔藓—积雪PBR材质制作流程

1. 运行 `python Tools/Unreal/Surface/GenerateMossPBRSourceArt.py --create` 生成五张可平铺源图（平台SourceArt）；后续运行 `--verify` 只读验收源图哈希、尺寸和拼接边界。
2. 默认运行 `python Tools/Unreal/Surface/AuthorMossSnowPBRAssets.py` 仅显示预定UE资产地址，不生成资产。
3. 在正式UE5.8编辑器的Python脚本环境中设置 `MOSS_SURFACE_MODE=apply`，执行同一脚本创建真实Texture2D（引擎纹理）、`M_GP_Surface_SnowMoss_PBR`（平台母材质）和 `MI_DBA_MossSnow_Cliff`（项目表现材质实例）。该脚本不修改原有雪母材质、正式Village地图或DedicatedServer（专用服务器）。
4. 设置 `MOSS_SURFACE_MODE=verify` 再在引擎中运行，验证真实资源类型、母材质连接数量和实例父级；运行 `python Tests/Assets/ValidateMossSnowCoverageResponse.py` 检查积雪与苔藓覆盖曲线。

**验收**：源图、脚本与引擎材质是三个不同门禁；Shader（着色器）、Client Cook（客户端资源烘焙）、Server Cook（专用服务器资源排除）、PIE（编辑器运行画面）与人工质量签收均需分别提供实际证据。
