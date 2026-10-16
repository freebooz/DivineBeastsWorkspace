# SSLM 新手村狼外观与动画蓝图制作规范

版本：2026-10-15（资源前置阶段）。本文件是制作合同，不是动画蓝图或运行验收凭据。

## 一、资产所有权

- BP_DBA_VillageWolf（新手村狼角色蓝图）：UE5.8已原生创建，位于 /DBAWorldPack_Village/AI/Blueprints，继承 ADivineBeastsVillageWolf（服务器安全狼玩法类）；不允许在它的默认对象上引用Fab狼网格、材质、动画或Niagara。
- DA_DBA_VillageWolfAI（AI数据资产）：UE5.8已原生创建，位于 /DBAWorldPack_Village/AI/Definitions。平台AI负责通用感知、导航与决策，项目层配置差异值。
- ABP_DBA_Wolf（狼动画蓝图）：应归以后合法登记的DBAContentPack_AnimalsProtofactor（通用动物美术包），必须采用真实狼Skeleton（骨架）制作，当前尚未创建。
- SkeletalMesh（骨骼模型）、Skeleton（骨架）、PhysicsAsset（物理资产）、PBR（物理材质）纹理和动画序列必须使用经过Fab授权导入的真实资源，禁止改名或文本冒充。
- 狼战斗规则由Dedicated Server（专用服务器）结算，客户端动画通知只能播放音效/特效，不决定伤害数值。

## 二、动画蓝图结构（真实Skeleton到位后制作）

1. Idle（待机）：循环呼吸、四足着地、自然耳/尾运动，可组合多段Idle。
2. Walk（行走）与Run（奔跑）：依据角色Velocity.Size2D()（水平速度，厘米/秒）驱动混合；初始阈值10和250厘米/秒，须按真实动画速度再校准。
3. Chase（追击）：使用平台AI复制的追击状态辅助提高运动强度，不允许客户端反向改变寻路目标。
4. Attack（扑咬）：单次攻击动画，受服务器确认的攻击事件驱动；AnimNotify（动画通知）只负责水花、狼吼、尘土等表现，不调用ApplyDamage（伤害结算）。
5. Hit（受击）：依据真实战斗事件表现受击，不通过本地计算生命值触发假伤害。
6. Death（死亡）：由ADivineBeastsVillageWolf::IsWolfAlive()（服务器同步生命状态）及死亡复制事实触发一次，停下移动并按4秒尸体寿命退场。
7. Ground IK（四足足底贴地，可选）：只修正客户端姿态，不改变服务器碰撞位置，远距离与低画质关闭。

每个动画检查循环标记、动画首尾、根运动、骨骼结构、旋转轴、动画轨道与LOD（网格细节等级）。不可使用Manny/Quinn人体骨架冒充四足狼Skeleton。

## 三、运行时接线

UDivineBeastsVillageWolfVisualWorldSubsystem（项目客户端狼表现子系统）在复制狼出现后，从GamePlatformData（平台资源服务）以世界租约加载SkeletalMesh和动画蓝图类，并设置到同一狼角色SkeletalMeshComponent（骨骼网格组件）。

实际资源路径需使用内容浏览器复制的对象路径：
- WolfSkeletalMesh=/真实挂载点/路径/SK_Wolf.SK_Wolf
- WolfAnimationBlueprint=/真实挂载点/路径/ABP_DBA_Wolf.ABP_DBA_Wolf_C

以上是路径格式说明，不表示这些资产已经存在。只有美术、动画、许可、资源加载及Client/Server Cook完成后才可分别批准bClientVisualsVerified和bEnableVillageWolves。

## 四、散布与性能约束

- Dedicated Server同时存活最多20只；生成锚点在水平面严格超过30米间距，玩家PlayerStart（出生点）周围22米禁止生成。
- 初始45米、八圈最远290米的有限候选通过NavMesh（导航网格）、地形坡度、胶囊占位进行确认；空间不足允许少于20只。
- 每批最多4只、每批间隔0.2秒；死亡14秒后补充，4秒尸体退场。客户端只消费复制状态，不使用PCG装饰假装服务器权威Actor。
- 20只狼同时运行时按实测显卡/CPU预算控制动画更新频率、骨骼LOD、阴影与Niagara，不能在没有实测时宣称达到特定帧率。
- 严禁因Fab骨骼、动画未就绪而生成不可见且具有攻击能力的狼。

## 五、本轮状态与后续

已保存的真实UE5.8原生资产：
- /DBAWorldPack_Village/AI/Blueprints/BP_DBA_VillageWolf（服务器狼角色蓝图）
- /DBAWorldPack_Village/AI/Definitions/DA_DBA_VillageWolfAI（狼AI定义）

已有作者脚本：Tools/Unreal/Creatures/AuthorVillageWolfCoreUE58.py（蓝图/数据资产生成）、VillageWolfAuthoringSpec.json（制作合同）、InspectVillageWolfVisualSourcesUE58.py（狼美术来源检查）。

尚缺：外部Fab狼骨骼、材质、动画蓝图、客户端实际视觉验证，以及正式新手村导航与联机验收。用户本轮不要求编译验证，因此不在本轮启动UBT、Cook、自动化测试或真实多人运行。
