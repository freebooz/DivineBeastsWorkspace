# 通用鸟群VFX交付收尾执行计划

> 执行方式：当前会话分步实施，逐步验证并记录；不重复重建已完成资产，不代替用户人工验收。

**目标：** 收紧鸟群发布范围，复核实际资产、场景和运行证据，准备人工验收。
**架构：** 平台GamePlatformVFX（唯一特效机制）；项目WorldDefinition（物种定义）与场景组件；仍走Presentation→Data资源租约→VFX→Niagara。MobaCommon不增加自然环境逻辑。
**工具链：** 锁定UE5.8、HWebCodex、UE内置Python和PowerShell 7，复用现有构建/Cook脚本。
**规格依据：** `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Docs/BirdFlock.md`、`Docs/Implementation/BirdFlockP4P7Execution_20261011.md`及现行`AGENTS.md`。

## 全局约束和并发边界

唯一工作空间DivineBeastsWorkspace，初始main提交bd106b3259c1a1917c0ece46d3ec2a8edd227fd4。其他任务正处理Fauna群落、飞鸟身份/所有权和UI颜色，本轮不重写这些C++或UI资产。保留所有未提交修改，不提交推送、不新增播放器、不关闭他人编辑器。

原始资源及历史uasset不删除、不改名。只收窄客户端烘焙白名单，并以真实容器门禁阻止旧模型入包。所有新增一方文件中文说明。新增文件同步总体目录规划。真实人工审美、目标设备性能和当前未运行项分开记录。

## 审查重点

旧网格ubulk/uexp附属块及大小写/Windows路径不能绕过门禁；缺必需资产、错误平台、空CSV、未知目标不得通过；源美术和开发审核地图不得发布；Flight正式模型及19项运行资产不能误排除；并行源码变化时历史报告不能冒充当前验证。

## S0：基线核验和计划落盘

- [x] 确认分支、HEAD、未提交修改、磁盘空间和构建进程，读取现行规则及鸟群记录。
- [x] 确认另一个Fauna任务拥有飞鸟身份/生命周期修复，本轮避开同一源码。
- [x] 保存本计划及执行台账，建立一次持久Goal和任务卡。

## S1：历史资源安全排除及严格发布门禁

文件：`Tools/Unreal/VFX/BirdFlock/ValidateBirdFlockContainer.py`、`Tests/Assets/TestBirdFlockReleasePolicy.py`、两个客户端CustomConfig的DefaultGame.ini、`Game/Config/DefaultGame.ini`、`Build/Game/CookFrontEndClient.ps1`。
接口：保留`validate(csv_path: Path, target: str) -> dict`及原CLI参数；默认严格，不增加绕过开关。

- [x] 编写并执行19项完整/缺项、旧网格与附属块、源美术、审核地图及外置对象、大小写/分隔符、未知目标、错误平台、空CSV/列缺失回归。
- [x] 旧实现真实失败后整改；发布策略最终18项测试全部通过。
- [x] 两个客户端仅收录Definitions、Niagara、Materials、Textures、Meshes/Flight、Blueprints六个子目录，原始模型不删除。
- [x] 正式默认排除`/Game/Development/BirdFlock`审核目录。
- [x] 严格门禁和Cook入口连接完成，保留UI/前端门禁；补齐子进程证据目录创建后，真实连接3个用例通过。
- [x] 新门禁正确拒绝历史真实包中的7个旧模型（19项正式资源无缺失）；历史报告未覆盖。

## S2：真实资产与场景只读复核

新增`Tools/Unreal/VFX/BirdFlock/AuditBirdFlockDelivery.py`，只向`Saved/Validation/BirdFlockDelivery`写证据。

- [x] 独立UE5.8进程读取平台依赖、三种Definition/系统/Flight模型和蓝图，通过。
- [x] 三种正式Niagara闭包未见旧模型；7个旧模型AssetRegistry查询无包引用，源文件完整保留。
- [x] 两张审核地图读取通过；正式场景三个组件物种/数量正确、标识唯一。真实游戏场景生态效果仍由人工确认。
- [x] 源码/资源前后SHA-256一致，输出AssetSceneAudit.json；明确runtimeVerified=false。

## S3：运行回归与性能证据

复用`RunBirdFlockIntegrationReview.py`、`RunBirdFlockGPUReview.py`及客户端BirdFlock自动化。

- [x] 续接已核对最新模块与源码，并在独立图形进程执行正式链路：GPU15组及所有权9项通过，运行窗口内189输入无变化。最终Client构建d706fa7b通过。
- [x] 三鸟种×0/1/32/128/512全部匹配，StopAll活跃Niagara为0且世界清理通过；新离线Stage原生3项成功、0失败，官方报告及进程Exit0已独立读回。
- [x] 已记录CPU/GPU及并发环境；本轮未取得当前运行帧耗，不宣称目标设备性能达标。
- [x] 未擅自扩建完整Boids或影视级近景羽毛，保留独立设计和人工优选边界。

## S4：独立Cook/Stage与容器验证

- [x] 原UI报告缺失问题已解除：真实RHI先失败，定位测试误初始化CDO而非CommonInput模块运行实例；最小修复后真实颜色测试和VerifyOnly通过，未降低任何颜色断言。
- [x] 官方CookFrontEndClient新RunId f0e7c867-1d0f-4d56-98a8-8462b2c20fac成功，19项必需鸟群资产齐全、旧模型/源美术/审核地图均0；前端134项通过。
- [x] 新Cook期间166项输入无变化、UI前后门禁有效。原失败证据保留；本轮已取得真实新包，不再仅依赖排除配置。
- [x] 历史实际服务器容器经严格门禁复核通过，鸟群资源0；本轮不修改服务器装配，未重新烘焙服务器。

## S5：交付记录与人工验收准备

- [x] 同步鸟群说明、总体目录规划、人工验收单，已执行与阻塞分别记录。
- [x] 输出BirdFlockDeliveryExecutionResult_20261011.md及真实证据索引。
- [ ] 人工签审：新手村/开放世界/选角鸟群密度、高度、遮挡、振翅及目标设备帧耗；无人签署时P7仍待确认。

## 顺序与回退

S0→S1定义发布合同，S2验证依赖不被切断，S3验证运行，S4验证新包，S5汇总。S3/S4依赖外部构建或UI门禁时先继续独立工作，不重复启动或绕过。回退只撤销本轮精确差异；禁止reset/clean或整文件覆盖其他任务变更。

## S3/S4本次续接结果

技术收尾完成；P7人工签审与目标设备性能仍待确认。续接证据见`Saved/Validation/BirdFlockDelivery/ResumeS3S4_20261011`；新包路径和失败恢复说明见同目录计划的ExecutionResult文档。原生测试先3/3成功，临时PowerShell汇总字面量错误单独保留，最后通过官方结果文件和元数据独立核验，不把包装报错改写成原生失败或伪造重跑。

## 实际执行台账

S0/S1/S2已完成。S1策略18项及Cook连接3项通过，历史真实客户端负例拒绝7个旧模型，历史服务器清单复核通过；S2三种正式资源和两图UE只读检查通过。
S3等待并行Fauna源码编译后的当前运行验证；S4等待有效UI颜色RHI报告及新Cook；S5资料已整理，人工签审未执行。
编辑器工具已切换至edit_project_files；Village配置和总体目录文档因原有混合换行被结构化编辑器拒绝，采用已观测SHA校验＋独占文件锁精确编辑，原始字节和变更证据保存于Saved，未格式化或覆盖其他内容。
