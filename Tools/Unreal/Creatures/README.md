# SSLM 狼怪物原生资源制作说明

本目录为新手村狼在UE5.8中创建服务器安全蓝图、AI数据资产以及后续美术接入提供受控作者入口。资源所有权不反向穿透到平台层，也不在Dedicated Server中使用Fab第三方模型。

- VillageWolfAuthoringSpec.json（狼资源制作合同）：定义最大20只、生成间隔超过30米、AI参数、原生蓝图路径与尚缺的Fab资源输入。
- AuthorVillageWolfCoreUE58.py（UE5.8蓝图与AI数据资产作者）：显式使用 -SSLMVillageWolfAuthoring 与 -ExecutePythonScript=脚本绝对路径，可以在真实UE编辑器新建两份原生资产；普通Python默认只读显示计划。拒绝覆盖非本作者资产。
- InspectVillageWolfVisualSourcesUE58.py（狼模型与动画输入审核）：只有Fab真实导入并填写Mesh/Skeleton/AnimBlueprint路径后才允许继续，缺素材就报告阻塞，不更改安全开关。

本轮已经在实际UE5.8中创建：
/DBAWorldPack_Village/AI/Definitions/DA_DBA_VillageWolfAI（AI数据资产）
/DBAWorldPack_Village/AI/Blueprints/BP_DBA_VillageWolf（服务器安全狼蓝图）

尚未取得Fab ANIMAL VARIETY PACK中的狼骨骼、动画和材质，故不会自动生成虚构的ABP、插件内容包或修改正式Village地图；bEnableVillageWolves和bClientVisualsVerified继续关闭。

本轮按用户要求不开展编译验证、Cook或联网测试。

## 正版Fab资源导入后的受控制作顺序

1. 将 ANIMAL VARIETY PACK 通过真实Epic/Fab账户的My Library下载到UE5.8工程的一个独立临时目录，不通过外部网页复制预览网格。
2. 录入 LicensedAnimalVarietyImport.json（Fab来源、授权证明在Saved目录下的路径、源UE对象路径、真实狼动画蓝图及6类关键动画）。
3. 在UE5.8 Editor运行 StageLicensedAnimalVarietyUE58.py，先由inspect显示缺失项；资料完整时运行prepare生成待挂载插件描述，重启后运行migrate将完整目录迁入DBAContentPack_AnimalsProtofactor并正式登记。
4. 在UE5.8 Editor运行 BindLicensedVillageWolfVisualUE58.py，只将真实骨骼网格和AnimBlueprint生成类路径回写配置。
5. 更新许可与作者记录，保留bEnableVillageWolves=false及bClientVisualsVerified=false，完成后续真实动画状态机、UE编译、客户端/专服Cook和联机验收才能由负责人启用。

当前用户禁止编译；上述两套作者脚本尚未对Fab资产执行，不能把计划目录当成已创建的第三方内容插件。
## 2026-10-16 后续增量

- StageLicensedAnimalVarietyUE58.py（授权动物包准备/迁移脚本）可以先迁移真正Fab狼网格、骨架与六种动画序列；源Fab无PhysicsAsset或AnimBlueprint不是迁移的否决条件，缺失项必须进入后续人工制作。
- AuthorLicensedWolfAnimBlueprintUE58.py（狼骨架动画蓝图底稿作者）：在Fab真实资源已经迁入内容包、无原动画蓝图时创建绑定真正Skeleton的原生ABP_DBA_Wolf；不会创建假状态机，也不会执行编译。默认inspect，只在UE5.8授权作者进程apply。
- WolfAnimationGraphRecipeV1.json（狼动画图规范）：待机/行走/奔跑/攻击/受击/死亡及转移条件的结构化输入；当前Status=Prepared_NotExecuted，不能当作已生成Blueprint节点。
- BindLicensedVillageWolfVisualUE58.py（客户软资源引用绑定）可读取源包自带动画蓝图或后续创建的ABP_DBA_Wolf；同骨架检查通过后仅写软路径，不启用狼群布尔开关。
- InspectVillageWolfVisualSourcesUE58.py（统一Fab资源检查）只读取LicensedAnimalVarietyImport.json作为实际来源清单，避免多套来源事实不一致；PhysicsAsset是可选后补项目。
- 专服狼群子系统已加强WolfSkeletalMesh/WolfAnimationBlueprint合法路径和动画资源包前缀检查；客户端额外检查UAnimBlueprintGeneratedClass与SkeletalMesh的Skeleton身份匹配。
- 本轮不进行任何编译、Cook及自动化测试。Fab正版资源尚缺时不得创建空通用动物包、伪造狼网格/动画或强开透明敌人。
## 资源接入后的补充约定

授权动画和模型导入后，StageLicensedAnimalVarietyUE58.py（Fab资源迁移工具）同时维护VillageDevelopmentClient和FrontEndClient两份客户端Cook资源路径：

- /DBAContentPack_AnimalsProtofactor/Animals（动物原网格、纹理、骨架与动作）。
- /DBAContentPack_AnimalsProtofactor/Wolf/Animation（基于真实Skeleton新制作的狼动画蓝图）。

客户端一次性申请GamePlatformData世界租约，20只狼复用资源；骨架不一致时拒绝配置并释放资源。人工美术可在Game/Config/DefaultGame.ini取消注释三项校准键：WolfVisualYawDegrees、WolfVisualScale、WolfVisualHeightOffsetCm。它们只调整客户端SkeletalMesh，不改变服务器胶囊或命中判定。

服务端缺Fab和客户端审核时会在世界开始日志记录VillageWolfInactive（狼群未激活）；该记录不能代替实际UE运行验收。当前用户不允许编译、Cook或自动化测试，所有本轮C++变更均待独立验收。



