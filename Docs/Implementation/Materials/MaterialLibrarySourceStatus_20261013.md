# 通用材质函数库 P0—P10：代码、蓝图与资源优先实施记录

> 记录日期：2026-10-13；工程：DivineBeastsWorkspace / Game/DivineBeastsArena.uproject（正式UE5.8主工程）。
> 用户明确约束：先完成制作代码、原生蓝图、材质资源与演示，再统一执行Editor/Client/Server构建、自动化、Cook及人工验收。本文记录制作状态，并不代替最终验收。

## 已有真实UE原生资产证据（本轮续建前）

1. 首次UE5.8 Asset Registry（资产注册表）盘点：437项相关资产；Surface既有7个MF（材质函数）+2个M（母材质）+1个MPC（材质参数集合）共10项实际存在且原生类别正确，证据为 MaterialAssetInventory_UE58.json。
2. 第一批5个Surface加3个VFX共8项MF以及8个独立QA MateriaI（审核材质）真正创建、保存、回读；经Monolith 0.23.0在UE5.8内报告8/8着色器已编译、无材质图问题。该批后相关注册表扫描453项。证据为 MaterialReviewShaderEvidence_UE58_20261013.json。
3. 第二批环境、侵蚀、水体、植物和VFX 18项MF及对应18个QA Material已由引擎真实创建；Monolith batch_recompile（批量重新编译）反馈18/18成功，尚未执行每一份QA图的独立性能分析。证据为 MaterialBatch2ShaderAndUIEvidence_UE58_20261013.json。
4. 另有5项GamePlatformUI（平台UI插件）通用材质函数真正通过Monolith创建并保存：圆角矩形、渐变背景、径向冷却、描边发光、按钮扫光。UI域QA母版的第一次FunctionCall（函数调用）接线失败，尚未获得真正MD_UI（界面材质域）的Shader编译证据，因此只能标“原生函数已保存”，不能升为“UI效果验收”。
5. 上述26个已编译函数对应的是隔离的开发审核母材质，不证明正式Village（新手村）、MainArena（主竞技场）、Niagara（粒子系统）、双端Cook、GPU性能或美术人工签收通过。

## 此轮源码和可执行算法配方（已真实写入仓库）

| 资源/入口 | 作用 | 状态 |
|---|---|---|
| Tools/Unreal/Surface/AuthorMaterialLibraryDomainUE58.py | UE5.8原生材质函数单一安全作者：inspect/apply/review/verify/compile（默认只读），按三层资产归属定位 | 源码完成；apply未执行 |
| MaterialLibraryDomainRecipes_Core.json | 基础函数、天气雨滴与地形：19个独立算法输入和真实表达式 | 配方代码完成；UE二进制未制作 |
| MaterialLibraryDomainRecipes_World.json | 水体、瀑布、植被：12个函数 | 配方代码完成；UE二进制未制作 |
| MaterialLibraryDomainRecipes_Surface.json | 建筑道具、石材、木纹、VFX、贴花：19个函数 | 配方代码完成；UE二进制未制作 |
| MaterialLibraryDomainRecipes_Project.json | 十二生肖角色项目层：10个独立表现函数 | 配方代码完成；UE二进制未制作 |
| MaterialLibraryLayerProfiles_V1.json | 12种Material Layer（材质层）不同PBR表面参数 | 配方完成；ML原生资产未制作 |
| Tools/Unreal/Surface/AuthorMaterialLibraryLayersUE58.py | 12个原生MaterialLayer（材质层）+1个共享MaterialLayerBlend（混合层）作者；已改用UE5.8专用Factory，避免错误原生资源类型 | 源码及工厂修复已写入；UE apply未执行 |
| Tools/Unreal/UI/MaterialLibraryUIMonolithRecipes_V1.json | 尚缺7种GamePlatformUI界面函数的输入、Custom HLSL和连线图 | Monolith配方完成；UE二进制未制作 |
| Tools/Unreal/UI/PrepareMaterialLibraryMonolithActions.py | 将P6剩余7个UI MF配方转成Monolith逐条制作/保存/回读步骤，支持双输出UITransitionDissolve（转场溶解） | 源代码已在工作区创建；没有启动引擎或编译UI材质 |
| Tools/Unreal/UI/AuthorMaterialLibraryUIReviewUE58.py | 12项UI通用函数的独立UserInterface域测试母版制作：原生FunctionCall、UV/时间、颜色与透明输出，禁止覆盖正式UI | 源码完成；12个真实UMaterial审核资产尚未制作或编译 |
| Docs/Implementation/Materials/MaterialLibraryUIWidgetSpec_V1.json | 真实WBP_DBA_MaterialLibraryReview的声明式控件树：83个唯一控件ID、12个Image样板、返回按钮和操作反馈 | 规格已完成；不等于WBP已生成 |
| Tools/Unreal/UI/AuthorMaterialLibraryWidgetMonolith.py | 复用既有Monolith协议和DivineBeastsDemoHubWidget父类创建项目第三层真实Widget蓝图，先检查资产存在性 | 作者源码完成；UE未运行，未生成WBP |
| Docs/Implementation/Materials/MaterialLibraryUIActionQueue_V1.json | 7个UI函数共42条Monolith制作/回读动作；已声明检测存在、仅新增、不覆盖和中止策略 | 机器动作队列已落盘，全部标记未执行；不存在新生成的7个UE材质资产证据 |
| Docs/Implementation/Materials/MaterialLibraryMapActionQueue_V1.json | 从正式Blueprint/UI资源Manifest生成Surface、VFX、Project、UI四张独立审核地图的Monolith原生创建前置检查和建图动作 | 4张.umap执行队列已写入项目，UE地图实际制作仍未执行 |
| Tools/Unreal/Surface/AuthorMaterialLibraryDemoUE58.py | 三类非UI隔离关卡：Blueprint展示架、球体、灯光、中文标牌及真实材质绑定 | 源码完成；地图及展示架未由UE创建 |
| Docs/Implementation/Materials/MaterialLibraryBlueprintAndUIManifest_V1.json | 4张独立审核地图、Actor蓝图和项目UI Widget蓝图的清单、职责与路径 | 合同完成；真实蓝图和地图未创建 |
| Tools/Unreal/Surface/AuditMaterialCapabilityUE58.py | 真正UE AssetRegistry逐项检查201项原生资产，生成JSON+中文索引，不伪造Shader或人工签收 | 审计源码完成；scan未执行 |
| Tests/Assets/TestMaterialLibraryDomainRecipes.py | 60函数、12材质层、7UI、冻结合同与演示路由源码测试 | 测试源码完成，按用户要求最终统一执行 |
| Tools/Unreal/Surface/AuthorMaterialLibraryStandBlueprintUE58.py | 真实UE Actor蓝图PreviewMesh组件与ReviewMaterial软引用、ReviewTitle中文文本字段作者；存在时只读验证，最终统一编译 | 蓝图成员字段的原生创建逻辑已补齐，尚未在UE实际执行并保存BP .uasset |
| Tools/Unreal/Surface/AuthorMaterialLibrarySourcePhaseUE58.py | 顺序调度60项领域MF、12项材质层、混合层、Actor蓝图及审核材质，并允许resume-from断点续建；不触发统一编译 | 已写入真实工作区，待UE5.8编辑器恢复后运行 |
| Build/Game/ServerPresentationExclusions.json、Game/Config/DefaultGame.ini | 开发材质审核目录不进入正式Client/Server，增加VFX及UI函数服务器排除 | 策略真源与默认配置已更新，生产Cook未执行 |


注：不同资源的物理分类遵守既有GamePlatformSurface（平台表面）、GamePlatformVFX（平台特效）、GamePlatformUI（平台用户界面）及DBAPresentationPack_Core（第三层项目表现内容包）。不创建GamePlatformMaterials（重复通用材质插件），不复制Niagara或天气权威状态，不往专用服务器引入纯表现模块。

## 与插件演示总览的实际代码接入

- 已增量修改 DivineBeastsDemoCatalogSubsystem.cpp（项目演示目录）中GuessOwningPlugin（审核场景所有者识别），将 /Game/Development/MaterialLibrary/{Surface,VFX,UI,Project}/（四类材质演示开发目录）分别归属原插件/内容包，不用新的插件注册器。
- 已增量修改 DivineBeastsDemoWorldSubsystem.cpp（演示世界子系统），UI审核地图走 /DBAUIPack_Core/Development/MaterialLibrary/UI/WBP_DBA_MaterialLibraryReview（Monolith应创建的专属项目UI Widget），其父类仍需为项目现行UDivineBeastsDemoHubWidget（演示基类）。
- 真正BP_DBA_MaterialLibraryReviewStand（材质审核展示蓝图）、WBP_DBA_MaterialLibraryReview（UI审查蓝图）及4张L_DBA_*_Review（审核地图）必须由真实UE5.8/Monolith创建；当前只提供完整配方与编辑器制作入口，不会将源码或配置伪称已生成.uasset/.umap。

### 本轮源码补齐与目录规划冲突说明

- 材质审核Actor Blueprint源码新增实际的 `ReviewMaterial`（材质软引用）与 `ReviewTitle`（中文可本地化文字）成员定义；有原生API不可用则明确失败，不在缺乏引擎时伪造成功。
- 为P6剩余7个UI函数准备42条按资产分组的Monolith实际动作，含单输出和`UITransitionDissolve`（有VisibilityMask与EdgeMask双输出）；每条操作只能在真实UE/Monolith成功后改变资产状态。
- `Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md`（全局目录规划）当前含其他会话留下的未解决Git冲突分隔符，**本轮没有修改该共享冲突文件**；这两项新增文件已经在本专项目录记录，待冲突由负责会话解决后再回填总体规划，不擅自覆盖其它业务文档。
- UI新材质、审核蓝图、四张开发地图、12项材质层仍处于真实UE待制作状态；尚未执行新增源代码的最终统一测试、Shader、Cook和人工视觉签收。
- 当前远程图形桌面只观察到WPS、Chrome、Codex及资源管理器窗口，没有正在显示的Unreal Editor；Monolith明确返回“Unreal Editor not running”。已对本会话三个陈旧编辑器作业发送终止请求，但Runner仍将旧作业计为running，未取得真正终止证明，不擅自结束其它会话任务。
- 此次依据UE5.8原生Factory接口修复12项材质层作者，变更仅为源码，尚未执行MaterialLayer及LayerBlend创建。
- 新增12份UI审核UMaterial作者、83控件的Widget蓝图声明式规格和Monolith作者；全部属于可执行源码，真实资源、UI主题绑定及视觉审批仍待制作和验收。
- 已补强AuthorMaterialLibrarySourcePhaseUE58.py资源阶段入口：仅允许锁定DivineBeastsWorkspace、UE5.8、没有PIE/Simulate且Asset Registry加载完成时制作；不会提前触发FINAL测试。
- 已扩展Tests/Assets/TestMaterialLibraryDomainRecipes.py源代码门禁：覆盖原生ML/MLB专用工厂、UI 12份审核母版源码、83控件与12个样板、真实C++ DemoHub绑定及错误状态必须带证据。按照用户要求本轮仅编写测试，不运行最终统一测试。

## 关键启动阻断及统一编译安排

- 在2026-10-13尝试重新启动正式UE5.8编辑器时，Log显示“Please build from an IDE instead.”及“FailedDueToEngineChange（引擎模块变化阻止编辑器自动构建）”，随即正常退出。
- 为优先制作资产，另外提交无编译启动参数的编辑器作业；必须以真实UE进程和Monolith状态检验结果，不能把Runner的queued/running或Monolith网关healthy直接等同已打开编辑器。
- 用户要求**先代码、蓝图、资源，最后统一编译验证**，因此本轮新增的60MF、12ML、7UI和所有新地图/蓝图不提前报告编译成功。若编辑器无法在不构建的情况下启动，将需要解决最小必需Editor启动构建阻断后才能继续制作二进制资产；此启动必需构建不能代替最后的Editor/Client/Server统一门禁。
- 所有已存在的.uasset/.umap禁止盲目覆盖、删除、重命名或移动；旧开发和其它会话未提交修改必须保留。
- 最终统一检查包括Python静态测试、完整UE Editor/Client/Server编译、功能QA Material编译、Niagara及UI域视觉、Development与Dedicated Server干净Cook/Stage、GPU实测以及人工明确签收。只有真实证据可改变生产可用状态。
- **2026-10-13编辑器恢复专项续建**：已新增 `Build/Game/InspectUE58EditorModuleState.ps1`（引擎及项目模块BuildId、DLL、进程和启动日志只读诊断），增量增强 `Build/Game/RecoverUE58EditorForMaterialLibrary.ps1/.cmd`（双击默认不主动构建、支持指定Editor模块最小构建、同步传递错误码和真实报告）；四个资源作者及串行调度入口均补齐正式工程真实路径与UE5.8版本校验。`AuthorMaterialLibraryDemoUE58.py`（演示地图作者）修复只要存在一个`MLReview_`对象就误报地图已完成的问题：逐一检查展架/标牌/灯光/地面与材质；半成品仅拒绝自动重叠、不擅自删除重建。相关静态测试源码已更新。以上是**源码与安全门禁已写入**，远端Runner仍由四个材质会话遗留作业占满，四项stop_job均确认已接受但随后仍显示running，Windows诊断作业仍queued；本轮尚没有新的Editor构建成功、Monolith正式连接、原生资产落盘或人工视觉验收证据。不修改201项资产状态。

- **2026-10-13剩余九阶段制作入口补齐**：新增统一资源调度 RunMaterialLibraryCompleteResourcesUE58.py、三层领域/材质层/审核材质分批原生入口 RunMaterialLibraryNativeBatchesMonolith.py、UI七函数Monolith队列执行器 RunMaterialLibraryUIProductionMonolith.py、四图安全创建/回读与逐图布局入口 RunMaterialLibraryReviewMapsMonolith.py、十二份UI审核材质的真正Widget Image Brush绑定入口 BindMaterialLibraryUIReviewBrushesMonolith.py，全部在既有 Tools/Unreal 子目录；既有通用函数、材质层、UI审核母版及Widget作者增加批次切片、UE5.8正式工程路径门禁、半成品拒绝跳过、物理包验证和重复执行保护。测试源码新增原生产阶段数量/顺序和资产归属校验。该批仅为可执行源码与创建入口，尚未获得Editor成功重启及Monolith返回真正项目会话的证据，故不可把60MF、12ML与共享Blend、7UI、12份UI QA、BP/WBP、四图标成已生成、已编译或已人工通过；完整Client/Server及Cook按用户要求后置。

- **2026-10-13物理目录盘点**：使用真实CwebCodex目录列表核对全部60个领域函数（Surface46、VFX4、项目10）、12材质层+Blend、7 UI函数、12份UI审核材质、BP、WBP及4地图目标路径，**98份目标新增原生文件均未在项目物理目录发现**，另60份待创建领域QA也未发现；已有不同路径的26份旧审核材质与5份UI函数予以保护。盘点详见`MaterialLibraryFilesystemAudit_20261013.json`，属于非原子远端文件系统证据，不能代替UE AssetRegistry或Shader验证。已在隔离镜像实际执行16/16静态契约测试，通过不等于UE原生资产制作完成。本轮原生分批入口另已增加对每批目标类、物理.uasset和基础表达式数量的UE内执行后检查。
## 当前范围总数和闭环要求
- 阶段工单的机器化执行顺序、未授权提前编译边界及每项实证要求归档于`MaterialLibraryResourceWorkQueue_20261013.json`（资源制作队列）。
- `MaterialCapabilityRegistry_V1.json`（201项索引）的60个领域MF配方、12个MaterialLayer与7个UI配方已逐项更新`SourceStatus`（源码状态），不会把仅有配方的资源标成已生成；`MaterialFunctionLibraryIndex.md`（201项中文台账）也已与当前真实状态同步。
- `Game/Config/DefaultGame.ini`（正式打包配置）已增加`/Game/Development/MaterialLibrary`（开发材质审核目录）NeverCook；`Build/Game/ServerPresentationExclusions.json`（服务器表现资源排除政策单一真源）增加该开发目录、平台VFX函数与平台UI函数的服务器排除。三个服务器受管INI需要在最后统一门禁阶段由既有政策生成器同步；未执行真实Cook。


201项候选能力由7个既有MF稳定合同、98个可独立MF候选、12个ML候选、24项应复用UE原生能力及60项母版/实例/Niagara/UMG/调试等其它实现方式组成。

98个MF中：**26个已在独立QA母材质内由UE编译验证；5个UI函数已创建但UI Shader待验；60个领域函数的源码和算法已经写入；另7个UI函数的Monolith节点图配方已写入**。算法和作者存在并不等于新.uasset存在。12个ML具备不同PBR参数及UE作者脚本，但原生MaterialLayer/Blend尚待创建。蓝图和4张审核地图尚待真正制作。

最终用户交付清单以 MaterialCapabilityRegistry_V1.json（201项机器真源）、MaterialFunctionLibraryIndex.md（中文201项索引）及实际UE扫描生成的 MaterialCapabilityAssetAudit_UE58.json（实时资源状态）相互核对；不得把“待编译/待人工审核”提升为“完成”。
