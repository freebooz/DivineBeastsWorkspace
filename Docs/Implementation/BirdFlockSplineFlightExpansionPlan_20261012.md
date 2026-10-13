# 《SSLM》正式鸟群群飞扩展实施方案与执行计划 V2.0

> 日期：2026-10-12
> 项目：DivineBeastsWorkspace（SSLM工作空间）／Game/DivineBeastsArena.uproject（正式游戏工程）
> 技术基线：UE5.8／Niagara GPU／三层单向插件架构
> 状态：规划待执行。本次仅编写计划文档，不代表代码、蓝图、材质、Niagara和实际编译已完成。
> 变更边界：不提交/推送，不碰其它任务当前未提交修改，不替换既有生产定义身份或重建VFX执行框架。

## 一、目标和明确不做的事情

目标：在现有正式GamePlatformPresentation（平台表现）→GamePlatformData（平台数据租约）→GamePlatformVFX（平台视觉特效）→Niagara执行链路之上，实现项目可复用的鸟群Spline（样条路径）飞行、批次生成/回收、三类Boids（群体行为）可选混合、GPU羽翼动画、跨距离表现分级、场景放置与插件演示中心交互验收。

演示中心能够直达鸟群专题，展示海鸥、燕子、雄鹰，在真实场景中切换路径、鸟种、数量、飞行/翅膀状态和质量档位。显示请求鸟数、真实活跃Niagara数量/GPU读回数与性能数据，不能把请求数量伪装成实际渲染数量。

不做：为每只鸟建Actor/Tick/SkeletalMeshComponent，不建立第48个代码插件、不重写Niagara执行器、Data资产管理器及新的演示VFX播放器；纯生态鸟群不改变服务器伤害、导航、碰撞与复制。旧AGamePlatformBirdFlockActor（实例化静态网格原型）仅保留已有兼容回归，不作为正式演示或关卡蓝图父类。

原需求要求先完成代码、蓝图和资源，最后集中编译/自动化/人工测试。只有原生UE必须载入新C++反射类型才能继续作者操作时，才允许记录明确的“生成资产所需最低限度Editor类型构建依赖”，不得将其声称为最终完整编译通过；若严格维持最后统一构建，则先用已编译的现有父类创建独立资产，最终编译后再绑定新增反射能力并复核蓝图。

## 二、现有工程确认与缺口

已经存在：
1. 平台GamePlatformVFX唯一VFX机制和鸟群GPU运动模块NM_GP_BirdFlockMotion、NS_GP_BirdFlock_Base、NET_GP_BirdFlock_Ambient以及M_GP_BirdFlock_Master；正式系统由Niagara Mesh Renderer每粒子显示一只鸟。
2. 项目DBAPresentationPack_Core拥有三种真实Niagara系统/WorldDefinition/Flight网格/材质、公共BP_DBA_BirdFlockAmbient。当前三种网格面向中远景，源RGBA中RGB存羽色、Alpha存翼尖振翅权重。
3. DBAClient/DivineBeastsPresentationClient中UDivineBeastsBirdFlockSceneComponent（正式鸟群场景组件）在BeginPlay通过StartAmbientBirdFlockForOwner提交请求；有Owner/RegistrationToken/MutationToken保证错误替换不破坏旧绑定、退出仅停止自身请求。
4. 当前WorldDefinition运行参数白名单为七项：User.BirdCount、User.FlockRadius、User.FlockHeight、User.FlightSpeed、User.FlockSeed、User.FlightMode、User.WindInfluence。平台FGamePlatformVFXParameters仅支持Float/Vector/Position/Color/Integer/Boolean值参数，没有通用UObject或Spline指针参数。
5. 三种鸟类支持现行盘旋、拉长闭合掠过、八字，单个本地玩家最多16群、数量0—512，已有0/1/32/128/512三物种GPU数量、600秒稳定性及Cook/Server剥离历史证据；这些证据针对扩展前版本，新增功能后须重新验证。
6. 新的插件演示中心DivineBeastsDemoWorldSubsystem::ApplyBirdCount/ApplyBirdQuality/GetActualBirdCount当前操作AGamePlatformBirdFlockActor旧ISM原型。需在接入本扩展时替换到正式UDivineBeastsBirdFlockSceneComponent/Presentation链路。
7. 尚未查到已交付的正式Spline群飞、完整Boids邻域、VAT多状态动画或全场景自主避障实现；不可将这些标记为现有能力。

UE5.8官方Niagara已有UNiagaraDataInterfaceSpline，可从世界SplineComponent采样位置、方向、切线和旋转，但官方仍标注为实验性；路径采样首先要在所用GPU平台验证准确性和World Partition/PIE隔离，再决定是否作为唯一执行方案。备选方案为在编辑器烘焙曲线采样表/LUT供GPU消费，避免GPU环境下临时世界Spline对象生命周期造成脆弱绑定。

## 三、架构与归属（不新增插件）

平台层：
- GamePlatformVFXClient（平台视觉特效客户端）：中立Spline路径采样合同、Niagara GPU运动子模块、群体行为、生成回收、动画参数与降级规则、效果预算/可见数量诊断；不出现Seagull/Swallow/Eagle等项目身份。
- GamePlatformVFXEditor（平台VFX编辑器模块）：曲线采样烘焙、材质/网格和Niagara Data Interface（数据接口）合法性检测、场景作者与自动化校验。
- GamePlatformPresentation（平台表现）保持中立语义与执行职责；GamePlatformData（平台数据）继续唯一资产ID/租约管理，不创建第二资产管理器。
- GamePlatformWeather/GamePlatformSurface/GamePlatformWorld（天气/表面/世界插件）只通过已批准的中立天气/环境事件适配，不产生VFX反向依赖。

MOBA通用层：
- 不新增鸟群机制、不默认加入MainArena竞技场、无赛事权威复制。若特殊竞技场需要演出，在MobaPresentation中提交已有中立表现请求，不引入鸟类资产或独立执行器。

SSLM项目层：
- DBAClient/DivineBeastsPresentationClient（项目客户端表现）：扩展既有UDivineBeastsBirdFlockSceneComponent与环境群落事务，增加可选Path/Spawn/Animation/Behavior配置的快照、校验和运行期更新；维持Owner和token、上下文代次、停止/回滚语义。
- DBAClient/DivineBeastsPresentationRuntime（项目表现运行语义）：保留项目鸟种到现有目录及Definition的映射，新增配置时保留稳定ID并评估必要的Definition版本迁移。
- DBAPresentationPack_Core（项目公共表现内容包）：三鸟种正式Flight网格、WPO材质实例及可选VAT纹理、Niagara系统变体、蓝图/数据资产；禁止平台硬引用项目资源。
- DBAWorlds/DBAWorldPack_Village（项目世界逻辑/世界内容）：按生态区域布置样条路径、触发范围与环境预设，地图只持有项目层资源及已批准通用组件；不在服务器地图加载纯视觉鸟群。
- DBAClient/DivineBeastsDemoClient（项目演示中心）：真实功能控制、WBP界面及审核关卡。必须改掉旧ISM鸟群演示路径，改走正式Presentation/Data/VFX。

## 四、关键机制及数据合同

### 4.1 路径引导

一个FlockGroup（鸟群组）绑定一个可选FlightRoute（飞行路线），组中心按样条参数u（0—1）沿路径移动，个体以不同初相位、局部偏移和轻量群体行为在组中心周围运动。路径不直接控制每只鸟的Actor；每个GPU粒子仍代表一只鸟。轨迹模式：原Orbit/Traverse/FigureEight（兼容）；新增SplineOneWay（单程）、SplineLoop（循环）、SplinePingPong（往返），以及Spline+FlockNoise（路径与群体扰动结合）。新增枚举值不得重排已发布0/1/2的旧值。

鸟群锚点持有路线引用/选项，平台仅接受World作用域可验证的中立RouteBinding（路由绑定）。不要往现有FGamePlatformVFXParameters中随意新增任意Actor/UObject指针。由VFX客户端私有执行器在已经受理的世界和Niagara实例上绑定Spline DI；路径Actor失效时取消或按Preset明确允许回退。GPU DI未验证时从USplineComponent于编辑器或事件阶段采样点/切线到曲线查表LUT，再交GPU采样，不要每帧从CPU更新512只鸟的位置。

重要门禁：路径数据必须有世界/关卡身份与代次、合法闭合标志、正长度、有限位置与切向量、可计算的高度边界；跨World Partition流送/PIE实例不能共享UObject指针或错误的本地坐标变换。大世界坐标（LWC）须在受控的局部空间计算以避免抖动；固定包围盒需覆盖轨迹加偏移和风扰动，不能只按一个球半径裁剪。

### 4.2 分批生成、循环与回收

定义SpawnPolicy（群组生成策略）：一次生成、时间间隔生成、触发生成；参数包含SpawnVolume（生成包围盒）、GroupCount（群组数）、BirdCountPerGroup（单群数量）、SpawnIntervalSeconds（间隔秒）、SpawnJitterSeconds（抖动）、MaxActiveGroups（最大同时群数）、PathEndAction（结束策略：Kill/Loop/PingPong/Wait）。现有每LocalPlayer最多16群、每群请求0—512是硬安全边界，不因本次扩展放宽；真实设备数量由EffectType和环境预算二次裁剪。

生成/回收由现有请求/Owner机制及世界作用域调度；建议用事件驱动计时器或统一有限数量的Group控制，禁止每鸟定时器。每次Spawn/重生具有世代身份、生成次数上限和终止条件；结束必须撤销上一次粒子再生成，不能依靠Emitter无限Burst导致鸟数增殖。保留已修复的EmitterState Self+Once与正式请求取消机制；不要把SpawnRate当作固定数量的无限生产开关。路径终点离场要同时处理生命周期、可视渐隐/回收及引用释放。

### 4.3 群体运动（分级而非所有群都跑全量Boids）

P0：默认沿Spline中心路径 + 每鸟稳定相位/半径/高度/速度扰动，极低成本。
P1：加入有限范围Separation（分离）、Alignment（对齐）、Cohesion（聚合），组长速度和Spline牵引优先；支持权重和最大转向/加速度约束。
P2：可选GPU NeighborGrid3D（空间邻居网格）或有界邻域采样，以及局部障碍避让/风险场。严禁直接全体成对O(N²)邻居检查；必要时先做少量组级避障或编辑器离线安全飞行走廊。避让属于可选客户端视觉能力，不承诺全场景精确碰撞。
飞行动态：方向跟随速度，盘旋倾斜取曲率并限幅；爬升/下降、风向/阵风和变速要连续平滑；组群分离再聚合需保持组身份和种子稳定可复现。位置/速度异常、零切线、重叠路径、快速改变种子均有安全退化和错误反馈。

### 4.4 鸟的网格与羽翼动画

首选沿用三鸟种正式Flight低多边形StaticMesh（静态网格），Niagara Mesh Renderer批量渲染，GPU材质的WPO（世界位置偏移）按Time、物种扇翅频率/幅度、ParticleID随机相位和飞行状态变形。翅膀必须围绕翼根合理旋转，不用整个网格上下来回平移。为每只鸟单独提供FlapPhase（扇翅相位），可选FlapFrequency/FlapAmplitude/GlideBlend/BankAngle；保留停拍滑翔与渐变状态，不允许全群同步抖动。

当前顶点色RGB承担羽色、A承担翼尖变形权重，绝不能直接将RGB改为左右翅控制而破坏既有三鸟种着色。确需左右翼独立旋转时使用额外UV、独立遮罩纹理、明确新增顶点属性或合法Morph/动画纹理，并保留旧WPO材质兼容版本及还原路径。

高级演出选用AnimToTexture/VAT（骨骼动作烘焙为顶点动画纹理），只对需要多姿态或近中景的鸟种启用。UE5.8的AnimToTexture属于实验性插件，必须先做导出/回读、贴图精度、UV索引、顶点排序和目标平台Shader验证；运行时只依赖已烘焙纹理与材质，不把编辑器实验插件或骨骼源资产随正式服务器打包。近距离少量可交互鸟才使用Skeletal Mesh/Animation Blueprint（骨骼网格/动画蓝图），这不是大鸟群默认架构。

### 4.5 距离级别、生态与性能

LOD近距离：少量高质量网格及VAT/必要时骨骼；中距离：正式低模Flight + WPO；极远距离：简化网格或Mask（遮罩）面片，按实际轮廓、透明过绘和硬件表现验证。LOD切换依据屏幕尺寸、镜头、设备及平台预算，不直接硬写未批准的米数阈值。

继续统一使用NET_GP_BirdFlock_Ambient（环境鸟群EffectType）的距离裁剪、数量、预算和可见性规则；不可建立第二套平行全局预算。Niagara System数量与鸟粒子数量要一起控制，组多时优先合并可行的相同物种实例；必要场景保持按区域裁剪避免一个超大bounds永不裁剪。指标包括CPU/GameThread、GPU Niagara、MeshRenderer、Shader采样、Overdraw、系统数、请求数、真正存活GPU粒子数、内存、资源租约和峰值。

生态放置：Village湖面少量海鸥低空掠过/逆风滑翔，桃林空地燕子中小群穿越，OpenWorld悬崖雄鹰稀疏大半径盘旋。MainArena默认不开装饰鸟群以保证竞技可读性；角色选择/创建的飞鸟必须避让角色、按钮与前景视角。世界分区卸载、昼夜与天气切换采用事件驱动刷新，不做每鸟联网复制。

### 4.6 数据资产与版本管理

现有三种正式DefinitionId：
presentation.dba.environment.birdflock.seagull@1
presentation.dba.environment.birdflock.swallow@1
presentation.dba.environment.birdflock.eagle@1
仍需完整可用。对新增可选字段采用向后兼容默认值，不改变原有已发布FlightMode数值、旧Niagara七字段语义、已批准字段身份及父类。只有确需破坏性Schema变更才创建明确@2 Definition与目录映射迁移，不在旧资产静默改动有效身份。

建议新增可配置字段（具体命名在实际类/Schema审查后最终锁定）：PathMode、PathSpeedScale、PathOffsetRadiusCm、SpawnMode、SpawnIntervalSeconds、ActiveGroupLimit、BoidsEnable、SeparationWeight、AlignmentWeight、CohesionWeight、FlapRateScale、GlideWeight、AnimationLODMode。不是所有字段都必须加入Niagara User参数：编辑器配置、生成调度、项目鸟种选择归各自所有者；只把GPU每粒子需要的少量稳定标量/向量放入平台VFXParameterSchema，严格检查类型、有限值、上下限和最大覆盖项数。Spline对象来源走独立经验证的Niagara DI绑定或采样资源，不作为普通Float参数。

## 五、实施阶段BF-P0—BF-P8：先代码/蓝图/资源，最后统一编译验收

### BF-P0 — 基线和技术原型审查
任务：对照现有GamePlatformVFX/BirdFlock文档与源码、三种Definition、正式Blueprint、源Blend/FBX和测试脚本，留存真实引用清单、当前7参数和值范围、Emitter循环状态、15组GPU与600秒旧版本证据；审查可否在现有VFX运行执行器中绑定Spline DI而不改公开中立请求结构；检查UE5.8 GPU Spline接口和AnimToTexture实验插件目标环境。
交付：BirdFlockExpansionInventory.json（资产/功能/依赖目录）、BaselineVsTarget.md（基线与差异）、变更文件所有权和风险门禁。
验收：确认正式Niagara链路是唯一执行；旧ISM演示被列为整改项，三鸟种原有功能与身份保留。

### BF-P1 — 路线数据合同与场景路径组件
平台客户端实现Spline路径参数/采样契约、合法性校验、世界作用域注册/解绑；项目在已有BirdFlockSceneComponent上增加可选Path配置/引用及运行前提，不改其无Tick、无复制和Owner/Token保障。设计可编辑BP_DBA_BirdFlockSplineRoute蓝图，利用USplineComponent提供控制点、出入场方向、闭合状态和偏移半径。实现无效Spline、零长度、跨世界、Owner失效与异步撤销的错误路径。
交付：平台路径合同C++、项目组件扩展C++、真实路径蓝图与相关配置资产。
门禁：非Spline旧鸟群运行逻辑和现有Definition仍不受影响；路径资源与实例有唯一世界身份。

### BF-P2 — Niagara GPU样条群飞模块
沿用NM_GP_BirdFlockMotion和三种现有项目系统；新增/扩展中立NM_GP_BirdFlockSplineFollow（样条路径跟随）与路径采样绑定，组中心u参数按弧长、速度和模式推进，个体在局部安全偏移内运动，保持X前向、Z上向与曲率倾斜。CPU/GPU两种采样或Spline DI与LUT回退进行独立行为比较后选择。校验固定Bounds、LWC、Spline关卡流送和NaN。
交付：真实Niagara模块/编辑器工具、Spline基础测试关卡及演示预设。
门禁：单程/循环/往返可重现，无抖动、穿越空路径或因缺接口产生静态假鸟。

### BF-P3 — 群组生成、循环与回收
在现有Owner/Request事务上增加有限群组调度（事件驱动，按Group/RequestId跟踪），支持生成盒体、批次、间隔、每批数量、终点停止/循环/回收、重启与取消。对刷新/替换和原World退出使用现有Generation/MutationToken，禁止新旧请求串改。保留Self＋Once单次Burst，定时生成新群时明确先终结或复用既有VFX实例。
交付：SpawnPolicy数据、调度C++与蓝图组件、Niagara行为扩展。
门禁：多批次不增生，0只清理，场景退出停止、重复请求幂等，最多16群/每群512约束保留。

### BF-P4 — Boids近邻规则与安全飞行
新增可选NM_GP_BirdFlockFlocking（群体分离/对齐/聚合）模块，先有界局部近邻，再评估GPU NeighborGrid3D；Spline牵引始终有优先级，可调转向上限、群聚距离、分离与滑翔强度。对地形、建筑、瀑布等优先选择设计师明确的安全飞行走廊；高配可启用有限GPU障碍查询，不承诺无漏洞碰撞。
交付：GPU群体模块、配置字段与静态/动态路线实验室场景。
门禁：单群、双群、近密群、反向流和高风扰场景不爆炸、不形成NaN、能聚散且恢复；没有O(N²)全体邻域循环。

### BF-P5 — 真实网格、WPO/VAT动画与LOD
复用三种正式Flight网格，检查羽翼根节点、顶点色RGB/A、UV和模型米厘米尺寸；对模型左右翼独立权重采用不破坏旧羽色的新增遮罩。实现WPO扑翼/滑翔/转弯三状态混合，各鸟独立相位，支持不同鸟种的扇翅频率和幅度。为近中景试做VAT（可选，先验证AnimToTexture），骨骼源仅保留在作者资源，不要求大群骨骼Actor。制作LOD样板、场景远景对比和不同Shader质量素材。
交付：源Blender与FBX修订、正式StaticMesh/材质实例、可选VAT数据纹理、绑定好的Niagara Mesh Renderer、材质/动画使用说明。
门禁：三鸟种轮廓可辨；不同鸟不齐刷刷扇翅；转弯/滑翔自然；不破坏现有羽色/翼尖变形/透明遮罩和Cook身份。

### BF-P6 — 项目资源和Village/OpenWorld落位
升级/新增三种鸟种Definition/场景预设，严格维持项目层资产所有权、稳定ID和Data租约；项目世界提供真实生态Spline路径、生成区域及默认稀疏群落。至少完成Village湖面海鸥、桃林燕子、OpenWorld高崖雄鹰三个可复用案例；角色选择/创建仅远景少量调用，同现有项目内容组合而不更改正式登录逻辑。
交付：DBAPresentationPack_Core项目Niagara变体、真实蓝图/数据资产；项目世界审核子关卡与生态预设。
门禁：项目三层依赖检查无平台反向引用；Dedicated Server不装配纯鸟群资源和视觉对象。

### BF-P7 — 插件演示中心独立鸟群实验室
复用当前Plugin Demo Hub（插件演示总览）。建立鸟群演示详情：鸟种、数量0—512、路径（盘旋/掠过/八字/Spline单程/循环/往返）、生成批次/间隔、风场、聚散权重、振翅与滑翔、LOD/画质档位；提供开始、停止、重置、单步观察、回总览、错误提示及真实性能数据。
必须将DivineBeastsDemoWorldSubsystem旧AGamePlatformBirdFlockActor路径改接正式UDivineBeastsBirdFlockSceneComponent、UDivineBeastsPresentationClientSubsystem；后台请求受理与真实GPU实际粒子数分列，接口拿不到GPU读回就显示“待测”，不可用旧ISM数量替代。演示UI蓝图通过Monolith MCP构建/保存/重载，不能脚本伪造uasset或文字假地图。
交付：WBP_DBA_PluginDemoBirdFlock、L_DBA_BirdFlockSplineReview、鸟群表现对比及项目正式链路集成场景、人工审核单。
门禁：开发模式无登录直达，真实链路切鸟/切路径/数量/速度/动画都可操作；离开后资源正确清理。

### BF-P8 — 最终统一编译、测试、Cook和人工验收
前七阶段完成源码、蓝图、真实资产与配置后，再统一安排代码结构/资产回读审查、完整Editor/Client/Server目标构建、UE Automation、Niagara GPU实跑、正式Cook与人工视觉验收。保持现有测试工作不受污染，命令/退出码/生成物与资源版本绑定留证。
必测矩阵：
- 鸟类：海鸥/燕子/雄鹰，每种0、1、32、128、512，并有多个群组组合与环境预算裁剪。
- 路径：三种旧路径、三种Spline新路径、路径无效/零长度/开闭、缺失源/WorldPartition卸载/PIE双世界。
- 动画：翼尖/左右翼、异步相位、滑翔、盘旋倾斜、距离LOD切换、VAT开关/回退；截图和GPU真实不同帧比较。
- 群体：分离/聚合/对齐权重边界、动态转弯、风、邻居网格上限、场景障碍物/安全走廊。
- 生命周期：重复Spawn/Despawn、终点循环、停止、错误输入、替换物种、同ID不同Owner、过期Token、账号/地图退出，600秒以上稳定性，不能超数增生。
- 性能：目标设备独占测试CPU/GPU帧耗、系统/粒子数量、Shader/WPO/VAT采样、显存与平台EffectType预算；绝对ms阈值以项目实测/批准预算为准，计划不虚构设备通过值。
- 打包：真正读取Client/Server Cook/Stage容器核查正式Flight网格/材质/Niagara/Definition完整、旧原型/源美术/审核地图被排除、Dedicated Server视觉资源0；不得仅依据ini规则声称成功。
- 人工：打开真实关卡，确认鸟群自然聚散、不同鸟种姿态、不会穿湖水/山崖/建筑、不会遮挡角色和HUD；审核人/日期/设备/截图/结论和待修项齐全。自动化通过不代表人工批准。

## 六、主要新增资产与建议路径（目前都是拟定，尚未实际创建）

平台C++优先在：
Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/{Public,Private}/Environment/BirdFlock/
（样条采样、路线合同、群体参数、调度与生命周期；真实路径在开发前按现有Public/Private组织确认，避免空目录）

平台Niagara/材质优先在：
GamePlatformVFX/Content/Environment/BirdFlock/Niagara/NM_GP_BirdFlockSplineFollow.uasset（Spline运动）
GamePlatformVFX/Content/Environment/BirdFlock/Niagara/NM_GP_BirdFlockFlocking.uasset（聚散规则）
GamePlatformVFX/Content/Environment/BirdFlock/Niagara/NM_GP_BirdFlockAnimationState.uasset（羽翼与滑翔状态）
继续复用NM_GP_BirdFlockMotion/NS_GP_BirdFlock_Base/NET_GP_BirdFlock_Ambient。

项目层资源与蓝图：
DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock/Blueprints/BP_DBA_BirdFlockSplineRoute.uasset（样条场景配置蓝图）
DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock/Definitions/（鸟种/路径/生成策略配置）
DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_{Seagull,Swallow,Eagle}.uasset（沿用实际物种变体）
DBAPresentationPack_Core/SourceArt/BirdFlock/（Blender/FBX、翼部遮罩、VAT原始数据、导出清单）
Game/Content/Development/BirdFlock/Maps/L_DBA_BirdFlockSplineReview.umap（真实独立演示）
Game/Content/Development/PluginDemos/UI/WBP_DBA_PluginDemoBirdFlock.uasset（Monolith制作的演示UI）

建议新增测试及交付文档：
Tests/VFX/BirdFlock/（样条/聚群/动画/生命周期/性能独立回归）
Docs/Implementation/BirdFlockSplineFlightExpansionPlan_20261012.md（本执行计划）
Docs/Implementation/BirdFlockSplineFlightExecutionProgress.md（实施时建立真实进度台账）
Saved/Validation/BirdFlockSplineFlight/（仅实际产生的UE报告和图形截图）

## 七、工序协调、风险和回退

实施顺序为BF-P0→P1→P2→P3→P4→P5→P6→P7→P8；必要时P4与P5可在不重叠源文件条件下进行，所有Monolith编辑器蓝图保存、资产重命名、统一构建、Cook/Stage必须串行。避免与其它天气、地形、VFX和UI工作修改同一关卡或母材质；开工先重新确认主分支和工作树脏文件。主工程名称、现有插件身份、DefinitionId与资产挂载点默认不改。

风险1：UE5.8 Spline Data Interface仍标实验性，GPU采样与源Actor生命期/路径转换不稳定；必须准备路径采样LUT回退而不是偷偷退回静态假动画。
风险2：Boids近邻复杂度或Niagara实例数超预算；先组中心路径、再轻量规则，最后按实测开启网格近邻，并复用平台EffectType裁剪。
风险3：顶点RGB颜色丢失或VAT顶点布局不一致；保留已批准Flight网格原资产和SHA校验，对新动画版本作独立资产比较。
风险4：旧ISM的Demo数量被误写成Niagara GPU实际数量；必须拆分RequestedCount、AcceptedRequest、ActiveNiagara、GpuLiveParticleCount四层状态，拿不到读回就不显示数字。
风险5：改动群落事务破坏同ID Owner/token和同步回调重入；锁定原有单测，新增版本迁移/并发/世界退出测试，禁止未经证据删除历史行为。
风险6：当前UE/Monolith环境若未能启动，应记录确切故障并继续非资产代码，待合法编辑器可用才生成真实.uasset/.umap；不得通过普通文件工具伪造二进制资产。

## 八、最终完成判定

1. 不丢失旧鸟群/既有三种物种及原有0—512数量控制、七项参数语义和正式资产身份。
2. 使用正式Niagara+Presentation/Data链路完成样条单程/循环/往返群飞，群体聚散与批次生成可独立配置、停止、重置。
3. 三鸟种低模网格可见、GPU羽翼异步扑翼/滑翔状态自然，近中远景可按设备性能选择LOD；VAT未通过实验门禁则明确停用并保留WPO正式回退。
4. 项目Village/OpenWorld至少具备真实可审核生态路径，第三层项目资产和平台通用运行层方向正确。
5. 演示中心使用正式BirdFlockSceneComponent，实际GPU读回与请求数量明确分离，可从总览一键进入真实鸟群实验室。
6. 完成真正的Editor/Client/Server编译与Automation，Client/Server真实Cook容器可核验，人工审查签署且失败项有定位；未完成不标为通过。

**执行授权边界：本文件仅为方案与分阶段执行计划；收到“按计划实施”后方对代码、蓝图、Niagara、纹理/材质及关卡执行对应变更。**
