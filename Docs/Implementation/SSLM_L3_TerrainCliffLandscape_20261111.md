# SSLM L3 山水地形、山体与峭壁艺术化实施规格

版本：V1.0｜日期：2026-11-11｜关联：SSLM全局美术艺术指导与视觉质量规范V4.0
状态：候选实施规格；正式地形集成需L1光照基线及L2材质候选通过后另行审批

## 1. 目标与适用范围
将用户确定的新手村湖边群山、峭壁参考图转化为可在UE5.8中实现、可探索、可测量的地形生产方法。目标是山体轮廓有气势、峭壁具有地质逻辑、山腰有可用平台、湖岸自然过渡、近中远景层次清晰，同时满足MOBA相机、路径、碰撞、导航和性能要求。
参考图只作为Village湖边群山和峭壁的专属美术基准；OpenWorld、MainArena及其他地图必须建立各自地貌与构图基准，不得复制新手村地形模板。

## 2. 前置条件与既有工程事实
- 复用Gaea 2.3高度图/Mask生产、Landscape多层PBR、Nanite岩石、RVT和World Partition/HLOD既有流程。
- 复用现有十层Landscape PBR生产资产和Terrain Function Integration候选，不新建第二套地形材质插件、天气系统或PCG权威。
- 现有历史报告曾完成40张CC0 PBR源纹理导入、十层母材质候选、四类PCG权重审核资源及隔离审核地图；但RVT正式绑定、PCG真实实例生成/烘焙、六态D3D12 GPU审核、完整Cook/Server剥离和人工签收仍有未完成项。使用前必须重新扫描UE AssetRegistry并核对最新证据，不能将历史报告当作当前状态。
- L1尚未由家中PC完成原生验收时，L3只允许地形灰盒、Gaea高度图/Mask检查、资产清单与候选关卡准备；最终材质和光照观感不得标记Accepted。
- L2材质未通过时，L3可使用既有已验证材质或中性灰盒，但不得以未批准Painterly PBR候选覆盖正式地形。

## 3. 地形构图与形态语言
### 3.1 大尺度轮廓
- 主山脉形成清晰天际线：峰顶高度、峰间鞍部、山脊方向和山谷开口具有变化，避免等间距圆锥峰、重复锯齿和明显程序化拼接。
- 峭壁以岩层方向、断裂面、节理、侵蚀沟、崩塌堆积和坡脚碎石建立地质逻辑；不能只靠陡坡灰度制造“悬崖”。
- 山腰平台必须有足够连续面积承载村落、农田、道路和战斗/探索空间；平台边缘要有自然过渡和安全边界。
- 湖盆与山体关系要解释水源、汇水方向、岸线高程、入水口/出水口及岛屿连接；不能出现无因悬浮岛或道路直接没入湖底。

### 3.2 中小尺度节奏
- 中尺度由山脊、岩台、支脉、峡谷、洞口、树冠群和建筑群形成。
- 小尺度由岩石碎块、侵蚀纹理、苔藓、草丛、土壤沟槽和水岸碎石补充；远景削弱高频细节。
- 近景强调可触碰材质和尺度参照，中景强调地貌体块与路径，远景强调山脉轮廓、空气透视和色块节奏。
- 通过道路、桥梁、台阶、山洞入口、古树和建筑屋顶引导视线；保留湖面、山谷、天空和草甸留白。

## 4. Gaea 2.3地形与Mask生产
### 4.1 高度图合同
- 高度图坐标方向、覆盖范围、像素尺寸、位深、单位、最小/最大高程和边界策略必须写入Manifest；与UE Landscape组件数、Section Size、Scale和导入Z Scale建立可复算映射。
- 高度图使用16-bit单通道数据，不得用普通8-bit预览图替代；禁止将带调色、标注、阴影或透视的效果图当作高度数据。
- 全局效果图与高度图必须是同一地形版本、同一坐标方向和同一范围；二者分开交付并记录共同版本ID与哈希。
- Gaea节点工程、Build设置、导出文件、尺寸、位深、坐标、导出范围和SHA256必须归档；未通过Gaea真实导出/导入回读的技术生成数据不得称为正式Gaea源。

### 4.2 Mask集合
至少建立：高度/海拔、坡度、坡向、曲率/凸凹、峭壁、山脊、山谷/汇水、草地、林地、竹林、桃花林/樱花林、银杏林、农田、道路、湖岸/浅水、洞口/建筑平台、Gameplay Exclusion。
- 每张Mask需说明生成节点、输入输出、参数、阈值、位深、通道、世界坐标方向和目标用途。
- Mask语义必须互相区分：道路与地表排除不能混为一张权重图；Gameplay Exclusion不等同于视觉地表层；水体/湖岸遮罩不能直接决定服务器水深或碰撞。
- 对10层Landscape权重，导出后检查尺寸、通道、像素范围、权重和/归一化规则、空白层和边界；保留原始Mask与UE权重图的映射清单。
- 采用坡度/高度/曲率等自动规则后，必须人工检查关键平台、道路、湖岸和洞口；程序生成结果需要支持局部手工修正。

## 5. UE5.8 Landscape、Nanite岩石与RVT
### 5.1 Landscape
- Landscape负责大尺度高度场、主要地表层权重、地形碰撞和可导航基础；导入前核对组件拓扑、分辨率、Z Scale、XY Scale和世界坐标。
- 十层材质按现有LayerInfo与权重图合同绑定；不要为追求视觉层次无节制增加材质层数、采样器或静态开关变体。
- 检查Landscape Layer Blend权重、Dirt降级层、空白权重和边界接缝；使用Debug View/Mask View逐层核验。

### 5.2 Nanite岩石
- Nanite岩石用于具有真实几何轮廓的峭壁、巨石、岩台和崩塌堆积；优先让几何表达大中尺度形体，材质只补充微观表面。
- 检查Nanite启用状态、Fallback Mesh、材质槽数量、碰撞复杂度、实例化、阴影、遮挡剔除和流送。
- Nanite不等于自动拥有合适碰撞、导航或性能；岩石群必须按玩家通行、镜头遮挡和战斗空间审查。
- 对重复岩石使用合理的资产变体、旋转/尺度范围和地质方向约束，避免明显复制粘贴。

### 5.3 RVT
- RVT仅在确实需要Landscape与道路/岩石/建筑/Decal的地表融合时采用；明确RVT Volume覆盖范围、Virtual Texture尺寸、物理池、采样材质和写入优先级。
- 先建立独立RVT实验关卡，验证写入者/采样者、Mip/页加载、World Partition流送、移动/目标RHI支持及GPU内存，再考虑正式地图集成。
- 不得因为存在RVT材质函数或配置项就宣称RVT已接入；必须提供实际Landscape/Rock/道路写入采样的UE资产回读和画面证据。

## 6. 游戏可达性与探索设计
- 每个主要平台、山路、桥梁、洞口、岛屿连接和湖岸入口均需定义玩家可达性、碰撞、导航、跳跃落点和相机观察范围。
- 峭壁可以作为视觉屏障，但不能在玩法允许通过的位置形成隐藏碰撞墙；不可达区域要有自然地形、植被或建筑边界解释。
- 道路坡度、台阶高度、桥面宽度、洞口尺寸和平台边缘安全范围按项目角色胶囊、移动速度、跳跃能力和相机距离实测，不在本规格中虚构固定数值。
- MOBA战斗区域和关键交互点需保留视线通廊；高草、岩石、树木、雾和建筑不能不公平地遮挡敌人、技能或危险范围。

## 7. 固定镜头与验证用例
- T01 Global Composition：俯视/斜俯全景，检查山脉、湖盆、平台、路径和主次景。
- T02 Cliff Silhouette：湖岸仰视峭壁，检查岩层、断裂、侵蚀和天空轮廓。
- T03 Mid-slope Village：山腰平台玩家高度，检查建筑、农田、道路和地形衔接。
- T04 Lake Shoreline：湖岸平视，检查浅水、湿岩、泥沙和植被融合。
- T05 Cave Entrance：洞口正面与侧面，检查入口可读性、遮蔽和通行空间。
- T06 Gameplay Camera：实际MOBA相机，检查敌我轮廓、技能、路径、战斗区域和地形遮挡。
- T07 Far View：远景山脉，检查空气透视、细节频率和轮廓层次。
- T08 Mask Debug：高度、坡度、曲率、峭壁、道路、林地、Gameplay Exclusion逐项可视化。
所有对照固定地图版本、相机/FOV、曝光、L1光照Profile、L2材质版本、分辨率、画质、RHI和目标设备。

## 8. 性能与技术证据
- 记录Landscape组件/Section数量、可见组件、纹理层数、材质采样、Nanite三角形/实例、岩石材质槽、RVT物理池和纹理内存。
- 记录GPU/CPU帧时间、Draw/RHI线程、显存/纹理池、World Partition加载、HLOD切换、Shader复杂度和阴影成本。
- 使用stat unit、stat gpu、ProfileGPU、Unreal Insights和目标设备捕获；固定相机、固定资产数量、固定画质和相同RHI比较。
- 真实Cook/Stage/Pak检查客户端需要的地形资产被正确包含、专用服务器不携带纯视觉资产；不能仅凭DirectoriesToNeverCook或JSON声明推断Cook通过。
- 原生Landscape、Nanite岩石、RVT资产和关卡需UE5.8保存并在独立Editor进程冷重载回读。

## 9. L3验收门禁
- Gaea工程/高度图/Mask版本、哈希、方向、范围、位深和UE映射可追溯。
- 山体与峭壁具有自然地质逻辑和明确前中远景构图，符合Village专属基准。
- Landscape层权重正确，无黑帧、错误空白层、接缝和明显纹理拉伸。
- Nanite岩石轮廓、Fallback、碰撞、导航、阴影和流送经过验证。
- RVT如被采用，必须有真实写入/采样、冷重载和目标设备性能证据。
- 道路、洞口、桥梁、平台、湖岸和探索路线在实际角色/相机下可用。
- 目标设备GPU/CPU/内存及Cook/Server剥离证据齐全。
- 美术负责人签收构图和材质；玩法负责人签收可达性/战斗视线；技术负责人签收原生资产和性能。
未满足任何一项，状态保持candidate或rejected，不得将候选地形覆盖正式Village地图。

## 10. 与既有工程资料的关系
- Docs/Implementation/TerrainProductionPBRExecution_20261016.md（历史真实制作及阻断证据；必须保留其未完成项）。
- Docs/Implementation/TerrainMaterialFunctionIntegration_20261017.md（平台MF函数化候选、RVT未验及Server隔离边界）。
- Docs/Implementation/Materials/MaterialOwnershipAndContracts.md（三层资产所有权）。
- Docs/Implementation/Materials/MaterialLayerCompositionPolicy.md（雪/苔藓/湿润/积水覆盖物理规则）。
- Tools/Unreal/Terrain/AuthorTerrainProductionMaterial.py、AuthorTerrainMultiLayerMaterial.py及既有RunTerrainGaeaFullReview.ps1工具；执行前必须读取当前脚本和最新Manifest，不复制出平行作者链。
- Gaea 2.3项目工程、Mask/Height导出Manifest、Landscape LayerInfo和Nanite岩石资产；所有路径需从当前仓库/UE AssetRegistry回读确认。

## 11. 当前状态
L3规格与云端路线任务可以先行完成；真实Gaea导出、UE Landscape/Nanite/RVT绑定、PCG生成烘焙、冷重载、Cook、GPU数据和人工艺术验收仍须在家中PC的UE5.8/Monolith完成。本规格不表示原生资产已创建或正式地形已修改。