# Surface Impact Marks 实施记录（2026-10-12）

## 现场
- 唯一项目 DivineBeastsWorkspace，起始HEAD `bad8db3a63c7a24f2fee8257387153d41c376b0e`，main已有并发未提交资产/UE UI/PCG/插件演示改动；本任务保留现场，不执行reset/clean/commit/push。
- 沿用已存在 `GamePlatformSurface` 的 ClientOnly + Editor 双模块，不新增Runtime空壳/服务器模块，已有全局环境状态功能保持不变。
- 同引擎当前由另一个Workflow Session运行 `DivineBeastsArenaServer Win64 Development` 构建（Job `wc_job_2Yuz3OEh7aEmYk5c`）；本任务不干扰其锁或其他PID。本轮UE编译/资产生成实际状态以独立证据为准。

## P0 实施计划与P1源码（待真实编译）
- 已创建 `ImplementationPlan_20261012.md`。
- SurfaceClient新增 MarkTypes、MarkCatalog、IGamePlatformSurfaceMarkService、WorldSubsystem、蓝图提交入口、MarkDemoActor 和两组自动化源码。
- 每世界资产目录与五种材质走GamePlatformData普通资源租约，异步后才能生成Decal；有界排队／重复GUID／淘汰／淡出／世界退出清理；只将真实已生成事件广播给外部VFX/SFX。
- 现阶段源文件已落盘，**未因静态检查宣称真实UHT/Editor/Client/Server编译通过**。编译发现的API问题需按原日志修正。

## P2源资源与UE资产（部分完成）
- `GenerateSurfaceMarksSourceArt.py` 首次生成5×3=15张1024x1024源贴图及SHA256清单，第二次运行全部UNCHANGED、没有覆盖或漂移。
- `SourceArt/Marks/T_GP_Mark_{Bullet,Scorch,Slash,Fluid,Crack}_{Albedo,Normal,ORM}_Source.png` 是风格化开发预览资源，非正式摄影AAA图，不是Texture2D。
- 编辑器导入/材质创作 `AuthorSurfaceMarksUEAssets.py` 已写入，Python inspect模式exit0验证15份源SHA；**真实Texture2D/Deferred Decal材质实例/目录Asset正在等待UE构建，未宣布完成**。
- 源PNG的人工组合预览位于 `Saved/Validation/SurfaceImpactMarks/SourceArtReview.png`。确认五种形状可辨认，但没有UE光照及材质签收。

## P3演示和上层接线（待UE资产）
- `AGamePlatformSurfaceMarkDemoActor` 和 `AuthorSurfaceMarksDemoMap.py` 已实现；Python inspect模式exit0。地图未真正生成前不算可运行演示。
- 蓝图可用 `SubmitSurfaceMarkFromHit`，成功痕迹通过平台世界事件通知现有VFX/SFX调用方。尚未为十二生肖技能数据资产批量填正式Kind，也未捏造Chaos网络破坏或正式血液资产。

## P4已执行的检查
- `python Tests/Architecture/ValidateSurfaceImpactMarks.py`：4/4通过，覆盖双模块非服务器边界、五类别值检查、源图身份/透明与ORM/Normal统计、作者脚本／蓝图合同。
- `python Tools/Unreal/Surface/GenerateSurfaceMarksSourceArt.py`：15份第二轮全部UNCHANGED，成功退出。
- `python Tools/Unreal/Surface/AuthorSurfaceMarksUEAssets.py`：inspect exit0，15份SHA有效；不是Editor真实创作。
- `python Tools/Unreal/Surface/AuthorSurfaceMarksDemoMap.py`：inspect exit0，只确认目标身份和脚本参数。
- `git diff --check -- Game/Plugins/GamePlatform/Presentation/GamePlatformSurface`：当次exit0，无补丁格式问题（CRLF提示不等于编译失败）。

## 未完成门禁
- 正式完整Editor/Client/Server编译；真实材质/目录.uasset、Shader；演示.umap、人工视觉；Moba项目级具体技能映射；Server排除单源及真实Cook/Stage资源清单核验。
- 真实UE资产目录尚不存在时，不预先以Present登记Cook策略或伪造.uasset。待生成后独立调整服务器纯表现排除并跑策略回归。
