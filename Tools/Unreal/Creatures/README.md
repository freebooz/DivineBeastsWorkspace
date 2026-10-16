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

