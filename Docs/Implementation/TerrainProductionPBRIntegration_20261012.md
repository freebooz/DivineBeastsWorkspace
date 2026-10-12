# 《神兽联盟》十种正式PBR地形材质实施与验收记录

版本：2026-10-12；工作空间：DivineBeastsWorkspace（神兽联盟工作空间）；引擎：UE5.8；本次连接器：Cwebcodex；会话：wc_sess_ZPzXyb7oTYXIY1JR。

## 一、范围与不可破坏边界

目标是以真实同源扫描PBR替换十类旧技术校准地表纹理，形成10层Landscape、雪、苔藓、湿润和PCG一体化的独立审核场景。属于地形制作任务，不重建插件，不修改其他会话的登录、角色、UI、BirdFlock或PluginDemo模块，不结束其编辑器和构建进程。

三层归属：

1. GamePlatformSurface（平台通用表面插件）：已有MPC、雪/苔藓/湿润通用PBR资产与编辑器作者命令。
2. GamePlatformPCG（平台程序化生成插件）：复用Landscape采样和LayerWeight的通用语义Graph。
3. DBAWorldPack_Village（项目世界内容包）：保存十种源纹理、地形层身份和共享地形权威数据；DBAPresentationPack_Core（项目纯客户端表现包）：拥有高精10层PBR母材质/审核MI，硬引用真实平台雪苔表面纹理；/Game/Development/Terrain只放开发审查地图和控制MI。

MobaCommon（通用竞技层）无地形特殊行为，不增加空模块。正式 /DBAWorldPack_Village/Maps/L_Village_Start（新手村起始地图）现用M_DBA_Village_LandscapeBridge（共享中性雪层材质）保持不变；新项目高精材质不进入正式Village共享地图，尚未人工批准迁移。

## 二、十种正式扫描源

全部来源于Poly Haven CC0真实扫描，统一2K源、同源BaseColor（基础色）/nor_dx（DirectX法线）/arm（ORM遮蔽粗糙金属）/Displacement（微高度）：

- Grass（草地）：leafy_grass
- ForestSoil（森林腐殖土）：forest_ground_05
- Dirt（普通泥土）：park_dirt
- Rock（岩石地表）：gray_rocks
- Scree（山地碎石坡）：rocky_terrain_03
- Sand（湖岸沙地）：damp_beach_sand
- Gravel（砾石地）：gravel_ground_01
- RoadDirt（压实土路）：stony_dirt_path
- FarmSoil（农田土壤）：farm_furrows
- ShoreMud（湖岸湿泥）：brown_mud_03

文件检查：40张2K无损PNG共629.55MB，官方MD5、项目SHA-256、尺寸和角色均通过。高度图中有真实16位灰度PNG，也有上游以8位等值RGB/RGBA存储的微高度图，未伪称全部16位。三项原扫描UV风险需人工审核：Rock法线、Scree法线及RoadDirt基础色有较大边缘差异，需要2×2平铺、近景/远景RHI验证。

源文件原位替换已完成，目标目录：

`Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/PBR/{层名}`

每层现有4张真实源图。正式真源是 `Tools/Unreal/Terrain/TerrainProductionPBRManifest_v1.json`（40份来源URL、许可、官方MD5和SHA256）。历史 `TerrainSourceManifest_v1.json` 仍为V1地貌校准哈希合同，不改变其原字节；其旧技术贴图30条不再表示当前源图内容。

备份：`Game/Saved/Validation/TerrainPBR_20261012/Rollback_TerrainCalibration_SourceAndUEAssets.zip`，共61项（30原PNG、30原UE uasset、历史清单），压缩约13.8MB，CRC校验通过。不能在旧UE编辑器占用时恢复/覆盖。备份处于忽略的Saved目录，必须在确认资源交付后迁移到正式受保护备份库，不能因未纳入Git而误认为已永久安全备份。

## 三、现有成果与实际状态

完成：
- 40源图从官方获取、签名与读取测试通过。
- 旧文件完整备份；40新源PNG已经按同名覆盖，历史校准高程、9掩码、10权重不变。
- `AcquireTerrainProductionPBR.py`（扫描源获取、验证、回退与替换工具）。
- `ReimportTerrainProductionPBR.py`（UE原生Texture2D同路径重导入及设置验证，30个旧Asset和10个新Height）。
- `AuthorTerrainProductionMaterial.py`（客户端高精10层母材质/MI、可选微高度混合、AO与法线归一化、MPC雪苔湿润共用合同）。
- `AuthorTerrainProductionPCGReview.py`（独立审核地图中绑定四类PCGVolume和森林/岩石/湖岸/道路同源权重过滤Graph）。
- `RunTerrainGaeaFullReview.ps1 -ProductionPBR` 受控完整UE导入、编译、PCG、地图和只读回读执行链。
- `ValidateTerrainProductionPBR.py`、`ValidateTerrainProductionIntegration.py`（原始签名、三层隔离与验收状态）。
- 服务器策略排除 /Game/Development/Terrain；ServerPresentationPolicy配置一致、25项回归通过。

尚未完成，不得误报：
- 当前仅磁盘30个旧Texture2D uasset，UE内40项正式高清重导入/10项Height尚未通过；
- 正式客户端PBR母材质和MI没有真正由UE编译保存，Material和HeightBlend目前只是作者脚本；
- 四份PCG WeightFiltered Graph及独立L_DBA_TerrainMaskReview.umap尚未完成实际生成；
- PCG实际点生成、网格Spawn与Bake、雪/苔藓/湿润真实RHI图像、三档材质GPU时间、客户端与服务器最新Cook/Stage/UnrealPak均待执行；
- 第三方Gaea 2.3真正高程/Mask源未取得，现有校准源仍不能冒充正式Gaea；
- 真实人工最终美术签收未完成，`artApproval` 始终 pending。

## 四、编辑器阻塞及安全恢复

2026-10-12本轮UE5.8命令行编辑器尝试失败：其他会话向 `DBAClient.uplugin` 加入 `DivineBeastsDemoClient`（插件演示客户端模块），尚未有对应Editor DLL。真实日志 `Game/Saved/Validation/TerrainPBR_20261012/UETextureImport.log` 报插件模块缺失，随后自动构建以 `FailedDueToEngineChange` 退出。

在独立工作树 `.worktrees/v` 又检测到其它会话运行 `DivineBeastsArenaServer` 的UBT进程，本轮 `Build.bat` 独占前置门禁拒绝开始编译（未停止他人PID）。该障碍不是扫描素材损坏。不能通过编辑他人插件描述、清除模块、篡改引擎构建产物或使用假.uasset绕过。

恢复路径：等并行UE/UBT结束，正常完成主工程Editor Win64 Development模块构建；运行 `RunTerrainGaeaFullReview.ps1 -Apply -ProductionPBR -EngineRoot F:\UnrealEngine-5.8.0-release`；再在独立编辑器进程用 `-ValidateOnly -ProductionPBR` 回读。脚本默认仅输出规划，不做写入；用户当前授权覆盖仅限前述十类指定资产。出现Shader/API编译错误时按真日志逐项修复，不降低引擎版本或伪造通过。

注意：因用户要求直接覆盖了DBAWorldPack_Village原Texture2D路径，未来应审计服务器是否误Cook高分辨率源。原策略禁止笼统排除权威WorldPack目录，未在没有真实Server包引用闭包证据前擅自扩大NeverCook。实际用户批准的生产迁移应优先把高精MI作为客户端软表现装配并保留服务器中性默认底图。

## 五、人工验收门禁

逐层查看真实UE5.8导入后的材质：贴图方向、色温、法线方向、ORM通道、微高度与粗糙度。独立关卡至少有十层地形的坡面/平地/湖岸交界，以及Forest/Rock/Shore/Road四类PCGVolume绑定证据。对无雪、轻雪、重雪和干湿切换记录真实RHI原图；审查苔藓应具宏微变化，积雪不在垂直岩壁无条件铺满，岸泥光泽合理。

验收必须从正式Shader编译/图层UClass/PCG已生成/实际GPU图/客户端Cook/服务器Cook容器中分别取得证据，不能用源PNG对照图代替GPU画面。最终填写人工签收人、时间、截图、RHI模式、目标GPU和人工同意结论，再考虑影响正式新手村。

## 六、可复现的只读检查

```powershell
python Tests/Assets/ValidateTerrainProductionPBR.py
python Tests/Assets/ValidateTerrainCalibrationSource.py
python Tests/Architecture/ValidateTerrainMaskPCGProjectionV2.py
python Tests/Architecture/ValidateTerrainProductionIntegration.py
python Build/Game/ServerPresentationPolicy.py check
python Tests/Foundation/Scripts/TestServerPresentationPolicy.py
# 查看完整执行计划，不创建任何真实UE内容
& Tools/Unreal/Terrain/RunTerrainGaeaFullReview.ps1 -ProductionPBR -EngineRoot 'F:\UnrealEngine-5.8.0-release'
```

以上源校验成功不代表Shader/GPU/地图/Cook已经验收完成。
