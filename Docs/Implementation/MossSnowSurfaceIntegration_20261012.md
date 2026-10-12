# 通用雪景与苔藓PBR集成记录（2026-10-12）

## 三层逻辑和资产所有权

第一层 `GamePlatformSurface`（通用环境表面插件，ClientOnly／Editor）保留唯一状态结构、8项MPC（材质参数集合）合同、积雪/苔藓/湿润表面遮罩以及可选真实材质资产。本次新增 `M_GP_Surface_SnowMoss_PBR`（通用积雪苔藓母材质）制作链，不新建天气权威、不引入玩法属性、不扩充服务器复制消息。

第一层 `GamePlatformWeather`（平台天气插件）继续产生并复制权威天气快照；`GamePlatformVFX`（平台视觉特效插件）继续负责近景、远景和地面雪粉Niagara（粒子系统），不重复实现VFX播放器或世界状态。

第二层 `MobaCommon`（MOBA通用层）无本任务独立规则，不机械新增模块。第三层 `DBAPresentationPack_Core`（项目通用纯表现包）仅允许 `MI_DBA_MossSnow_Cliff`（神兽联盟项目材质实例）调整覆盖密度与坡度参数，不保存平台算法。共享 `DBAWorldPack_Village`（新手村世界包）保持服务器安全基础材质，不向专用服务器Cook（资源烘焙）直接添加客户端Surface/VFX硬引用。

## 源资源和实现

`Tools/Unreal/Surface/GenerateMossPBRSourceArt.py`（苔藓PBR源纹理生成器）以固定随机种子生成5张无缝源PNG：2048像素基础色、法线、ORM通道、微高度和1024像素宏观覆盖；清单 `MossPBRSourceManifest.json`（资产源文件校验清单）记录所有权、尺寸和SHA-256。已有不同素材禁止覆盖。

`Tools/Unreal/Surface/AuthorMossSnowPBRAssets.py`（UE5.8真实资产导入及Shader构图工具）只在正式工程与编辑器中工作，默认inspect（只读规划），apply（实际制作），verify（真实引擎资源回读）。新母材质使用世界XY宏观采样、几何顶点法线坡度遮罩和雪量连续阈值；颜色、法线、粗糙度、环境遮蔽完整混合，金属度为0，湿润只改变光学表现。现有 `MF_GP_MossLayer`（苔藓材质函数）保留旧调用方，不就地覆盖旧资源，增量提供独立的写实高质量母材质。

## 执行与验收边界

源图通过SHA和边界测试，仅代表源美术资源有效。真实Texture2D（引擎纹理）、Material（母材质）、MI（材质实例）须原生UE5.8导入、Shader编译、保存与重载验收。最终仍需对雪量0/0.23/0.85、垂直/25度/50度岩面、晴雪转换、苔藓岩缝覆盖、湿润光泽、地面雪粉与风向、不同画质的GPU过度绘制进行人工视觉及性能审查。

本次不主动覆盖正式Village共享地图、旧雪MI、旧材质函数及其他会话的PCG、UI、VFX修改。正式Server Cook不应含新增Surface纹理或项目纯表现材质；交付前需独立检查Cook闭包。

## 本次实际执行证据

- 平台苔藓源纹理5张：`GenerateMossPBRSourceArt.py --create`（生成）及 `--verify`（只读校验）均输出 `MOSS_SOURCE_QA_PASS maps=5`，包括尺寸、SHA-256及平铺接缝。
- UE5.8真实引擎资源：已导入5张Texture2D，创建 `/GamePlatformSurface/Materials/M_GP_Surface_SnowMoss_PBR`（含68个材质表达式）及 `/DBAPresentationPack_Core/Materials/Environment/MI_DBA_MossSnow_Cliff`（项目MI），引擎报告 `MOSS_SNOW_UE_ASSETS_VERIFIED ... nodes 68`，Commandlet结果为0。
- 独立编辑器审核地图：已创建 `/Game/Development/MossSnow/Maps/L_DBA_MossSnowReview`，引擎报告 `MOSS_SNOW_REVIEW_MAP_SAVED`，地图文件已存在；包含平岩、25°/50°坡面、垂直岩壁、天气审核控制器和默认光照。
- 数值回归：`Tests/Assets/ValidateMossSnowCoverageResponse.py` 返回 `MOSS_SNOW_COVERAGE_RESPONSE_PASS`；Python脚本编译及默认只读规划均通过。
- **未签收**：客户端实际PIE截图及操作体验、真实GPU性能预算、全工程Editor编译、Client/Server Cook闭包与真实游戏主城/新手村效果。源图和地图存在不代表这些测试已通过。
 
- 独立UE5.8进程二次只读回读：`MOSS_SURFACE_MODE=verify` 重新加载插件母材质与项目MI，输出 `MOSS_SNOW_UE_ASSETS_VERIFIED ... nodes 68`，UE Commandlet返回0。
- 插件目标平台静态隔离检查通过：`GamePlatformSurface`（ClientOnly/Editor）和 `DBAPresentationPack_Core`（仅Game/Client/Editor），结果为 `MOSS_SNOW_PLUGIN_ISOLATION_STATIC_PASS`。该检查不等同于Server Cook实际闭包。
- 当前组合母材质为桌面端／高品质可选方案：低画质或移动端应继续使用 `M_GP_Surface_Lite`（轻量母材质）或后续受控静态开关版本；不要将本组合材质硬编码到全项目材质。
