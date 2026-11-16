# SSLM 新手村脚步及水体空间音效专项补丁（2026-10-19）

## 一、基线和范围

基线：远端 `main` 于执行本专项时为 `f716a6f`；原始证据见 `Docs/Implementation/SSLM_AudioLibraryRuntimeIntegration_20261018.md`。本次在 `fix/sslm-village-footstep-water-20261019` 独立分支实施，避免覆盖本地冲突中的其他开发任务。

继续使用 `GamePlatformPresentation`（平台统一表现协调）→ `GamePlatformSFX`（现有音效执行器）和 `DBASFXPack_Core`（项目音频资源包）。不新建第二套播放器，不修改服务器权威战斗逻辑，不伪造地图坐标。

真实清单中有六种脚步语义：`Footstep.LeavesA`（落叶A）、`Footstep.LeavesB`（落叶B）、`Footstep.Gravel`（碎石）、`Footstep.Mud`（泥地）、`Footstep.Stone`（石面）、`Footstep.Wood`（木面）。原生循环音只有 `Ambience.WaterStreamA/B`（流水A/B）；`Water.SplashA/B`（飞溅）属于单次音效，严禁冒充无限循环瀑布声。

## 二、已经提交的独立源码与检查工具

1. `UDivineBeastsAudioFootstepNotify`（角色脚步动画通知）：真实射线碰撞命中后，按`PhysicalMaterialToClip`（物理材质资产名映射）→`SurfaceTypeToClip`（物理表面枚举映射）→`DefaultClipId`（受控默认脚步语义）的顺序解析；可以设置`TraceChannel`（射线检测通道），默认可见性通道。未命中地面则静音，不通过空中位置捏造材质。
2. `UDivineBeastsPresentationClientSubsystem::PlayAudioLibraryFootstepClip`（客户端脚步受控播放）：以真实角色`USkeletalMeshComponent`（骨骼网格组件）及足底Socket（脚骨挂点）为联合身份，同一脚掌85毫秒内防止动画混合层等重复通知触发两声。左右脚独立；只保留已提交成功请求的时间戳，使用弱引用，不维持退出角色存活，最多保留256个网格键；`StopAudioLibraryForWorld`（世界级音频清理）完整释放。
3. `UDivineBeastsWaterAudioAnchorComponent`（真实水体声音锚点组件）：正式共享地图**只存放真实水体Actor、`SSLM.WaterSource`的Actor Tag（对象标签）和原生`USceneComponent`（通用场景组件）标记点**，标记点需标记`SSLM.WaterAudioPoint`及唯一的`SSLM.WaterStreamA/B`音源选择标签。客户端`EnsureAudioLibraryForWorld`（世界音频就绪）按场景标签发现后调用`EnsureRuntimeAnchor`（运行时水声适配实例化），仅在客户端临时创建`ClientOnly`（仅客户端）水声组件，避免将客户端私有类序列化进服务器共享地图。组件位置由真实标记点绑定而来，不编造坐标；异步加载由`OnContentPackChanged`回调处理；`EndPlay`自动停止循环。
4. `Tools/Unreal/Audio/TestAudioLibraryBindings.ps1`（静态接线门禁）：增补脚步材质Surface回退、短时去重、世界清理、水声真实Owner标签、原生组件位置、音频Loop白名单、异步回调及退出取消门禁；拒绝锚点自行`SpawnSound`（直接播放）或`LoadSynchronous`（同步加载）。
5. `Tools/Unreal/Audio/AuditVillageFootstepWaterAudioUE58.py`（原生编辑器只读审计）：真实加载`/DBAWorldPack_Village/Maps/L_Village_Start`（新手村）并枚举本次已加载Actor，检查**原生通用场景标记点**的真实Owner、Tag、实际位置及受控循环音标签；同时冷读正式行走/慢跑动画的脚步通知。默认打印缺口，不进行资源写入；可通过两个独立环境变量将实际关卡与正式动画缺口改为硬失败。

这些改动仅补充第三层项目音频使用方式，不引入音频线程的新调度器、Ticker（逐帧任务）或新SoundWave硬引用。声学效果继续由音频资源包里的`SoundAttenuation`（距离衰减策略）和`SoundConcurrency`（并发控制策略）以及既有平台服务负责。

## 三、正式关卡水声布置（必须由原生Editor或Monolith在真实场景执行）

关卡：`/DBAWorldPack_Village/Maps/L_Village_Start`。2026-10-18留下的原生只读检查只加载到10个Actor，未发现经确认的水声锚点；不代表其他World Partition（世界分区）内容不存在水体。

步骤：先打开正式关卡及相关流送分区，逐个确认湖岸、溪流、河道或瀑布真实水体Actor及实际发声位置；在已核实的水体Actor上添加`SSLM.WaterSource`（水源标签），在该Actor上添加**普通UE `SceneComponent`**（场景标记点）并设置`SSLM.WaterAudioPoint`（声音标记点标签）；此标记点仅允许二选一：`SSLM.WaterStreamA`或`SSLM.WaterStreamB`。由关卡作者实测标记点相对位置，保存并冷重载复核，不能输入猜测的瀑布坐标。**切勿将`DivineBeastsWaterAudioAnchorComponent`这一ClientOnly组件作为原生持久资源保存到共享地图**；它只在客户端运行时自动创建。

根据现有声音素材实际试听选用`Ambience.WaterStreamA`或`Ambience.WaterStreamB`；轻流水和强水流只作为候选用途，两条音源目前并无已验证的专门湖浪/瀑布音色，不适合时须保持静音或另走已批准的音效资源新增流程。每处相互独立的真实水流可以拥有独立组件与循环身份，重叠区域需由声音设计师试听调整，不能批量铺满同一声音导致叠加。

使用现有 `/DBASFXPack_Core/Audio/Shared/Policies/S_DBA_AudioLibrary_Ambient_Attenuation`（原生环境音三维衰减资产）和资源定义上的并发策略。衰减半径、遮挡、空间感、峰值音量及听感平衡都须在实际编辑器/客户端中调整；此处不填未经测试的数值。

游戏客户端进入Village后，`EnsureAudioLibraryForWorld`会扫描本次已加载真实Actor的上述原生Tag并创建客户端水声适配。若World Partition流送分区后续才加载新的真实水源，应在分区加载完成的客户端事件中再次调用`BindTaggedRealWaterSources`（查找真实水体标记点）；**本轮尚未接入所有流送分区事件**，必须列为关卡集成待办。客户端组件无Tick，音频分类未Active时不会直接播放。

## 四、正式脚步动画和物理地面映射（尚待制作）

正式资产：
- `/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MF_Unarmed_Walk_Fwd`（行走）
- `/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MF_Unarmed_Jog_Fwd`（慢跑）

原`/Game/Development/Audio/Animations/AS_SSLM_AudioReview_Walk/Jog`（脚步通知审核副本）只有未签字的参考通知时间，不得据此直接写入正式动画。需逐帧审核左/右脚实际落地点并确认`foot_l/foot_r`骨骼或Socket存在，才可在正式动画轨道配置对应Notify。检查项目真实Landscape（地形）、地面Mesh（模型）和材料的PhysicalMaterial（物理材质）关联与SurfaceType（物理表面类型），按实际观察配置六种Footstep语义；不能猜测场景石材/木材/土壤的物理材质资产名。复杂材质交界、起步、停止、转身、跑跳以及多动画层时须重点试听。

## 五、测试命令与验收口径

PowerShell 7 静态门禁：`pwsh -NoProfile -File Tools/Unreal/Audio/TestAudioLibraryBindings.ps1`。

UE5.8需在能独占编辑器模块及资产的环境中编译`DivineBeastsPresentationClient`（项目客户端表现模块），然后通过现有UE `-ExecutePythonScript`（编辑器Python脚本）方式运行`Tools/Unreal/Audio/AuditVillageFootstepWaterAudioUE58.py`。设置`SSLM_REQUIRE_VILLAGE_AUDIO_ANCHORS=1`（必须有真实水声锚点）、`SSLM_REQUIRE_FORMAL_FOOTSTEP_NOTIFY=1`（正式动画必须有脚步通知）可分别将缺口提升为失败。脚本只检查当前加载Actor，仍需覆盖实际Streamed Levels（流送关卡）。

最后独立验收：正式UE5.8 Editor/Client编译；原生地图和动画资产保存及冷重载；真实客户端走/跑不同物理材质脚步试听；水面和瀑布近远距离三维声压变化；物体消失、角色退出、账号切换、断线重连、跨地图循环消失；双客户端与5v5 Audio Insights音频性能；Client Cook（客户端烘焙）含声音且Server Cook（专用服务器烘焙）排除纯客户端音源。每项保存运行日志与可重放证据，不得以静态文本门禁代替实际扬声器试听。

## 六、本次实际实施状态

远端独立分支已制作C++补丁和校验资源；**截至此文档撰写，未修改正式`L_Village_Start`原生地图，也未给正式动画添加Notify**。Cwebcodex本地主工作树存在3处冲突、263个变更路径及14个活跃任务；独立托管worktree不可用，因此未抢占现有UE Editor/资产锁。静态门禁需要以本分支实际检出后执行；原生编译、真实试听、距离衰减、Cook和性能压测均为**未验收**，不得宣称完成。
