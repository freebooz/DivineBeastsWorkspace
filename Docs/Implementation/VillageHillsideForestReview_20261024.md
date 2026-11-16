# SSLM 山坡森林独立审核（2026-10-24）

当前原生审核地图：`/DBAWorldPack_Village/Development/PCG/Maps/L_DBA_Village_HillsideForest_Review`。
仅使用本地工具与 Monolith MCP。未接入正式 `L_Village_Start`，等待人工视觉审核。

## 本轮结果

- 107棵原型树，中部 `|X|、|Y|<105m` 范围16棵。固定种子20261024，重复生成及保存、切图、包重载后实例变换指纹一致。
- 相对初始树体整体4倍，实例原生Scale为6.4—9.6；独立派生网格叶冠XY乘sqrt(8)，投影面积8倍，高度与主干保持。按用户最新选择允许树冠交叠；最小树干净距16.12m。
- 原生碰撞地面最大坡度34.8711°（门限35°），最大根部贴地误差小于0.000001cm；道路、建筑夹具及出生点、水域空间排除违规0。
- 四个共享简单不透明PBR材质实例：树皮、亮叶、花色、主体树冠；原始技术原型网格未覆盖。这是用户批准的技术原型，不是正式森林美术模型。
- 用户最新指定树冠改成桃花色，取代浅橘黄色：主体sRGB `#E8B6BD`、亮部粉白`#F1D5CF`、点缀柔粉`#D99EA9`，沿用V3桃花色板，转换为线性BaseColor后写入审核材质。树干`#685541`、粗糙度、107棵散布和4倍树体/8倍树冠均不变。原生截图为`Forest107_PeachBlossom_Overview.png`。
- 一条顺现有谷地流向中央湖泊的河流，以及一个低洼湖泊。复用内容包现有湖泊/河流蓝图、平台Surface水材质与原生Water插件；NoCarve子蓝图先保存重载再放置。湖泊24点、河流61点；河流约每2m采样碰撞地面，末段伸入湖泊轮廓。
- 水面使用Water插件原生持久静态网格生成，禁用地形雕刻、碰撞和导航影响。529个地形采样点在添加水体前后及重载后高度差0cm。天气、灯光、原有Actor变换未改动。

## 复用与实现边界

平台机制继续归GamePlatformPCG及既有Water/Surface；项目审核资产归DBAWorldPack_Village，不新增插件或运行生成器。`BP_PCG_Forest`、单个WorldDirector、一个全域原生生成图和8个空间参与者完成生成。PCG进行候选点生成、真实碰撞投射、坡度过滤、空间排除、随机疏散及ISM实例化，Python只装配和审计。

已有森林权重过滤图作为独立子图保留；审核Landscape未拥有其要求的LS_ForestSoil权重层，因此本次走显式有界Landscape分支，不为演示涂改地形。道路和建筑排除属于审核夹具，不能据此宣称正式村庄道路/建筑接入已验收。

Water材质保留项目材质父链，审核实例启用原生固定深度/流速分支，关闭WaterInfo顶点映射和动态边界裁剪。使用`SetWaterBodyStaticMeshEnabled`显式重建原生静态水面，避免只更新样条却仍显示旧网格。未改动引擎源码，也不依赖会在本机UE5.8 PostLoad中清除的旧WaterMeshOverride配置。

## 原生验证与证据

- 作者脚本：`Tools/Unreal/PCG/AuthorVillageHillsideForestReview.py`，入口`revise_scatter()`、`author_water()`、`regenerate()`、`audit()`；均保护审核地图身份及正式地图SHA256。
- `Saved/Validation/VillageHillsideForest20261024/Forest107_FinalReload.json`：107棵、16棵中部、零违规，原始Actor变换一致。
- `WaterFinalReload.json`、`TerrainBeforeWaterPlacement.json`、`TerrainAfterWaterPlacement.json`：原生河湖、无雕刻/导航影响、529点高度验证。
- `NativeAssetValidation.txt`：12个原生审核资产有效，0错误、0警告、0无法验证。
- `Forest107_Final_Overview.png`、`Forest107_Final_RiverLake.png`：保存重载后的UE原生1920×1200截图，没有图片后期编辑。
- 实例变换SHA256：`db044f8c2af41956a96bd3484c220ddf826b03fa3c83cd58e3039d6370b75953`。
- 正式地图SHA256保持`e1bc67e7a410d057bf2827d831ca401a77fe8ce938b60608a0069e122947f4b0`。

此次为编辑器独立审核，不触发Client/Server编译、部署、Cook或联机验证。树木复用ISM且不增加玩法碰撞，水面为原生烘焙网格；尚未做GPU实测，不宣称达到正式性能预算。人工视觉审核及正式地图接入仍待完成。

## 2026-10-24 林间黄色泥土小路

新增北侧横路、中部主路、湖西支路三条相连小路，长度约430.0m、401.6m、351.7m。路面核心宽4.4m、每侧1.2m渐变。基于9409个原生地形碰撞采样和107棵现有树干数据进行确定性寻路，绕开湖泊、河道、建筑夹具和树干，不移动或重新散布既有树木。

复用`DBAWorldPack_Village/Terrain/Textures/RoadDirt`已有扫描BaseColor/Normal/ORM及`GamePlatformSurface/MaterialFunctions/Layers/MF_GP_PBRLayerBlend`。浅黄色参数采用V3浅黄土地sRGB `#C5AD83`并转换为线性值。审核材质`M_DBA_Review_ForestPaths`由现有LandscapeBridge原生派生，四通道泥土混合接到原有积雪混合的基础输入；保留湿润、积雪、压实脚印、位移等输出连接，不更改共享母材质。

道路直接成为Landscape材质的一部分，没有额外道路网格、透明贴花、碰撞或导航修改。位置使用单张1024×1024线性灰度遮罩，UE负责真实Texture2D导入与编译保存；不是Landscape原生可刷权重层。路线JSON与数值遮罩源归内容包`SourceArt/PCG/HillsideForest/Paths`；原生GamePlatformPCGSplineActor同步道路排除到WorldDirector（现共11个参与者）。审核样条与遮罩为一次编辑器烘焙，调整路线需同步JSON、重烘焙/重导入遮罩，不能把移动样条误认为材质会自动更新。

新增作者脚本`Tools/Unreal/PCG/AuthorVillageForestPathsReview.py`：本地`plan()`生成数值权重，Monolith `author()`装配原生资产；已有材质时拒绝重复追加节点。地图、素材包重载后，107棵树的变换指纹保持一致，529个高度采样点差值0cm，所有排除检查通过；新增材质、遮罩及受影响PCG图共3项原生资产验证有效、无错误。未做Client Cook或GPU实测。

证据：`ForestPaths_FinalReload.json`、`ForestPaths_Verified.json`、`ForestPaths_NativeValidation.txt`。原生截图：`Forest107_Peach_Paths_Overview.png`、`ForestPaths_GroundView.png`、`ForestPaths_CentralTop.png`；均在`Saved/Validation/VillageHillsideForest20261024`，供人工审核。

## 正式新手村同步（2026-10-24，用户明确批准）

登录关卡差异的原因已确认：编辑器为独立审核地图，登录服务器及旧包为正式`/DBAWorldPack_Village/Maps/L_Village_Start`。两图地形采样最大差6389.81cm。用户随后明确选择“以编辑器设计为准，整体同步为正式新手村（包含地形）”，并要求保留原雪效果；此决定取代本轮前期暂不修改正式地图的限制。

正式关卡身份和登录准入链保持既有地址。地图通过UE原生保存同步，原地图、审核地图及原雪母材质的字节备份及SHA位于`Saved/Validation/VillageHillsideForest20261024/BeforeFormalPromotion`。原生另存时编辑器发生异常退出，恢复后重新加载正式关卡独立检查：107棵树、河湖、小路均存在，144个真实地形碰撞采样与审核设计最大差0cm。此处以独立重载结果作为资产证据，不把异常的保存调用结果记为通过。

森林运行资源由Monolith原生迁入`/DBAWorldPack_Village/PCG/HillsideForest`，正式地图不再引用被发行配置排除的Development资产。河湖子蓝图继承原生WaterBodyLake/River，继续NoCarve和NoCollision，不再依赖专服已排除的旧纯表现父蓝图。纯客户端水材质归既有`/DBAWorldPack_Village/Water/Materials/PCG`，未被正式地图使用的小路审核母材质归`/DBAWorldPack_Village/Development/PCG/ForestPathMaterials`，复用现有Server排除策略；不能仅迁走父资源而把子材质留在AlwaysCook目录。客户端泥土纹理及样条数据归现有Village内容包，未新增插件。

小路接入稳定的`M_DBA_Village_LandscapeBridge`，保留积雪覆盖、SSS、蓬松雪高度、位移和5cm脚印压实接口。共享中性桥使用UE基础混合节点，避免硬引用ClientOnly Surface材质函数；客户端雪纹理租约和Nanite MID桥继续消费原路径。RoadDirt使用共享Wrap采样、路径遮罩使用共享Clamp采样，编辑器编译统计为14个sampler；首次SM5打包超限被门禁拦截后已修正，必须由新的Cook日志证明发布着色器通过。

两处出生点仅抬高Z到新地面以上，XY与旋转保留。导航范围扩至560m×560m、竖向240m并执行原生RebuildNavigation。`StartVillageDevelopment.ps1 -HoldHeavySnow`使用原GameMode和平台权威天气服务驻留大雪（SnowAmount .85、SnowIntensity .90），不重播四阶段测试、不修改认证或出生门禁。该开关仅非Shipping、真实Village Dedicated Server且unattended时受理。

Client/Server完整增量构建已通过，正式双端Cook与资源门禁通过，源码/资产/实际收据及Stage映像绑定通过。证据位于`Saved/Validation/VillageHillsideForest20261024/PublishedVillage`：Build `c8864b68-9bfb-4281-8042-ccf73eedc099`，Client Cook `4be76acf-b9ed-4ebe-9e5a-9ad019603c1e`，Server Cook `3f749c27-cb1d-439d-b35e-1089d9c06df3`。实际Village及MainArena均已替换为此Server包，战斗输入指纹`5cda65a7a9237f63061c2109f67cf69550ba9fd2005f9f8f1375fce0873196a9`。

`VillageDevelopmentServer/DefaultEngine.ini`通过UE原生CookerSettings排除`WaterBody`渲染Actor；此类水面已明确NoCarve/NoCollision，不承担权威。平台`GamePlatformWaterRegion`独立继承Actor，保持双端权威数据；仅此开发Server配置受影响，Client/Editor水面仍保留。实际Server Cook及独立容器排除门禁通过，不能将放宽NeverCook或引入Surface客户端资源作为修复。

两个新客户端PID49064/49088均从正常登录页启动，未自动认证，FPS限制0。用户已手工登录第一个客户端，真实Village准入成功，无网络版本错误；原生画面可见森林和地面积雪，雪Nanite MID64槽及10cm位移回读成功。第二客户端仍为登录页，尚未完成双客户端同时入场或人工视觉签收。原先误启用的离线审核入口、配置及客户端均已撤回。原审核作者脚本的旧正式地图SHA保护保持有效，防止升正式后仍无意修改共享资源；后续继续编辑需显式建立新的审核基线。

最新用户指令改为“关闭雪”：已移除本次验证启动的强制大雪参数，使用同一Server包重启Village为默认晴天（初始SnowIntensity/SnowAmount均0，无调度或预设），不改变MainArena及雪资产。证据`PublishedVillage/SnowDisabled.json`；原先森林积雪截图仅证明关闭前的雪链路有效，不代表当前天气。已入场客户端需重新登录接收新世界快照。
