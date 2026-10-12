# Surface Impact Marks 实施记录（2026-10-12）

## 现场
- 唯一项目 DivineBeastsWorkspace，起始HEAD `bad8db3a63c7a24f2fee8257387153d41c376b0e`，main已有并发未提交资产/UE UI/PCG/插件演示改动；本任务保留现场，不执行reset/clean/commit/push。
- 沿用已存在 `GamePlatformSurface` 的 ClientOnly + Editor 双模块，不新增Runtime空壳/服务器模块，已有全局环境状态功能保持不变。
- 并发Server真实目标 `wc_job_2Yuz3OEh7aEmYk5c` 曾运行并由其所属会话请求停止，终态stopped/exit-1；本任务未中止。当前另一个会话的original Editor构建`wc_job_bpEZDUjb0oIyKKvR`及v验证曾运行；后续须审查真实目标和产物时间，不能重复发起占锁动作。

## P0 实施计划与P1源码（待真实编译）
- 已创建 `ImplementationPlan_20261012.md`。
- SurfaceClient新增 MarkTypes、MarkCatalog、IGamePlatformSurfaceMarkService、WorldSubsystem、蓝图提交入口、MarkDemoActor 和两组自动化源码。
- 每世界资产目录与五种材质走GamePlatformData普通资源租约，异步后才能生成Decal；有界排队／重复GUID／淘汰／淡出／世界退出清理；只将真实已生成事件广播给外部VFX/SFX。
- 现阶段源文件已落盘，**未因静态检查宣称真实UHT/Editor/Client/Server编译通过**。编译发现的API问题需按原日志修正。

## P2源资源与UE资产（部分完成）
- `GenerateSurfaceMarksSourceArt.py` 首次生成5×3=15张1024x1024源贴图及SHA256清单，第二次运行全部UNCHANGED、没有覆盖或漂移。
- `SourceArt/Marks/T_GP_Mark_{Bullet,Scorch,Slash,Fluid,Crack}_{Albedo,Normal,ORM}_Source.png` 是风格化开发预览资源，非正式摄影AAA图，不是Texture2D。
- 编辑器导入/材质创作 `AuthorSurfaceMarksUEAssets.py` 已写入，Python inspect验证15份源SHA。隔离工作树真正Editor运行`author-materials`进程`wc_job_uLHIVj1wEyl3KUEK`退出0、脚本标记`SURFACE_MARK_UE_MATERIALS_PASSED`，原生生成15个Texture2D、7节点Deferred Decal母材质、5个MI；所有21份真实`.uasset`无覆盖传回原工作区，逐个SHA/实际二进制头比对21/21通过，总计6,080,060字节。原工作区证据`Saved/Validation/SurfaceImpactMarks/AssetTransferAudit_20261012.json`；尚无新的Catalog及演示Map。
- 源PNG的人工组合预览位于 `Saved/Validation/SurfaceImpactMarks/SourceArtReview.png`。确认五种形状可辨认，但没有UE光照及材质签收。

## P3演示和上层接线（待UE资产）
- `AGamePlatformSurfaceMarkDemoActor` 和 `AuthorSurfaceMarksDemoMap.py` 已实现；Python inspect模式exit0。地图未真正生成前不算可运行演示。
- 蓝图可用 `SubmitSurfaceMarkFromHit`，成功痕迹通过平台世界事件通知现有VFX/SFX调用方。尚未为十二生肖技能数据资产批量填正式Kind，也未捏造Chaos网络破坏或正式血液资产。

## P4已执行的检查
- `python Tests/Architecture/ValidateSurfaceImpactMarks.py`：最新7/7通过，覆盖客户端／普通Game／Editor与Server隔离、五类别值、源图SHA及透明/法线/ORM统计、项目显式命中适配、两阶段作者脚本和无覆盖物理包阻断，以及三个Client Cook闭包/三个Server剔除。旧4/4、6/6为历史阶段证据。
- `python Tools/Unreal/Surface/GenerateSurfaceMarksSourceArt.py`：15份第二轮全部UNCHANGED，成功退出。
- `python Tools/Unreal/Surface/AuthorSurfaceMarksUEAssets.py`：inspect exit0，15份SHA有效；不是Editor真实创作。
- `python Tools/Unreal/Surface/AuthorSurfaceMarksDemoMap.py`：inspect exit0，只确认目标身份和脚本参数。
- `git diff --check -- Game/Plugins/GamePlatform/Presentation/GamePlatformSurface`：当次exit0，无补丁格式问题（CRLF提示不等于编译失败）。

## 本轮继续执行与断连恢复
- Cwebcodex已重新关联 `wc_sess_dHNl8eUjUoKRJBCK`。main从最初bad8db3a递进到f53b3fa2；新增BirdFlock/Snow等其他会话变更不执行reset、clean或强制终止。
- Surface客户端新增的跨Game开发目标采用ClientOnly Module及`Game/Client/Editor`明确白名单，Server保持排除；第三层`DivineBeastsPresentationClient`仅依赖平台公开Mark合同，提供五种受控命中语义和真实WorldStatic/WorldDynamic判定，拒绝未确认/无碰撞/角色命中，不虚构正式十二生肖技能表。
- 真实资产原工作树首次进程 `wc_job_mPJepm4_oEylp2tC`：引擎在执行Python前因`DBAClient`描述新注册的`DivineBeastsDemoClient`未在现有Editor二进制中找到而退出1；本任务创建痕迹uasset为0。原始输出 `Saved/Validation/SurfaceImpactMarks/UEAuthorMaterials_20261012_A/`，未将失败记为成功。
- 通过同一主工程的隔离检出`.worktrees/v`运行正式Editor资产作者脚本：复制17个输入文件5,328,270字节、旧Editor DLL加载成功、导入15张Texture2D、母材质7节点、5个实例，UE退出0。21个实际二进制均带UE Package标志`c1832a9e`，字节哈希对比通过后才迁回original；无冒用v的Server构建，记录失败的首次original启动原因。
- `Build/Game/ServerPresentationExclusions.json`已将现实存在的`/GamePlatformSurface/Marks`登记为AllServers纯表现排除，官方受管工具`generate`精确更新三套Server NeverCook和DedicatedServer Pak规则；`check`通过，策略25/25测试通过。同时FrontEndClient、VillageDevelopmentClient、PluginDemoClient各加入一次Marks `DirectoriesToAlwaysCook`，不写入共享默认Game；7项架构/路径测试通过。**配置和脚本回归不等于真实Server Cook或完整Client资产打包成功**。
- 最新C++ UHT/UBT、Catalog `.uasset`和独立演示 `.umap`仍未完成；并发original Editor构建由其他会话掌握，其编译证据需独立核对，不能将旧DLL、源图检查代替本次编译。

## 续执行记录：当前编译阻塞与隔离回退
- 最新复核提交 `8d2bfe2738b25c72640475840348a885e0628e05`，其后存在其他任务的少量未提交文件；没有回滚／清理／覆盖原有资产，也没有本任务主动提交或推送。21个Marks原生包仍在原工作区。
- 本次新增的 `Tests/Architecture/ValidateSurfaceMarkCookExclusion.py` 运行3/3成功：Marks物理包UE头、三类Server NeverCook、Pak精确目录、客户端模块不进Server均校验。此后补充一项 **只在真实Review Map生成后强制运行的边界测试**；未制作地图前按测试跳过，不能记地图已剔除。生成地图后还必须把 `/GamePlatformSurface/Maps` 写入单源并重新generate、check、跑真实Cook。
- Python作者 `AuthorSurfaceMarksUEAssets.py` 将真实Catalog验证升级为：五类恰好各1项、预算合法、每个Kind的材质软引用必须指向对应的已导入MI、尺寸/持续时间/淡出参数合理；不因资产存在就默认判通过。更新已精确同步到隔离验证检出 `.worktrees/v`，无材质或贴图覆盖。
- Module构建第一次PS7参数边界测试：把两个模块误传为逗号分隔单字符串，被 `OnlyModules` 正则在执行前拒绝，exit1；没有启动UBT或改源。第二次按PS7数组调用、RunId=`eeb9e75b-f428-46d2-9bb1-d60dfd38ca5c`，Job=`wc_job_kXPCzb7E1-IP7nSG`，真正产生UBT日志；当前停在读取配置后的共享引擎UBT互斥等待，**尚无C++成功或失败编译证据**。其它Session同时有完整Editor构建及运行Editor，未干涉；已在Session板对相关会话发出协调信息。
- 为不覆盖或重编译原项目当前DLL，已将13个新增SurfaceClient C++源文件按新增-only同步至 `.worktrees/v`，其SurfaceSettings头和实现采用精确补丁加入Marks软目录设置。回退选项是在引擎锁释放后隔离编译并真实制作Catalog/Map，但不能将v验证编译冒充原工作区全目标成功。

## 未完成门禁
- 正式新增代码的Editor/Client/Server UBT构建；目录`DA_GP_SurfaceMarkCatalog.uasset`、演示`L_GP_SurfaceMarks_Review.umap`、D3D12实际视觉；自动化/PIE、正式技能表绑定、真实Cook/Stage及容器资源清单验收。
- 已交付21个真实纹理/材质UE资产且具物理字节证据，但目录与演示需要新UCLASS完成Editor编译后才能真正创作。Shader脚本执行过材质图重编译，尚未获得实际独立GPU可见性及正式AAA美术批准。
