# 神兽联盟通用材质函数库建设（2026-10-13）
> **最新制作进度与真实验收边界**：`MaterialLibrarySourceStatus_20261013.md`（源码、蓝图资源优先制作清单）。首批8个MF及第二批18个MF已由UE真实生成、隔离Shader验证；另5个UI函数已保存但UI Shader未验。剩余60个独立MF、12个材质层、7个UI函数配方及独立演示蓝图/地图已完成制作代码，但**真实二进制资产尚未生成**。最终全目标编译、双Cook、GPU与人工签收均推迟到资源制作完成后，不能以作者脚本当作交付。
> **执行任务索引**：`MaterialLibraryResourceWorkQueue_20261013.json`（先源码/蓝图/资源，最后统一测试的16项阶段任务）；`MaterialFunctionLibraryIndex.md`（201项逐条实时实施状态），机器真源`MaterialCapabilityRegistry_V1.json`（归属、完成度及建议UE路径）。Cook排除已写入受管政策真源及DefaultGame.ini，尚未生成最终Server Profile或实际执行Cook。



本目录承担材质能力归类、UE资产实际审计、三层插件所有权、参数合同、表现组合、质量与验收证据。真实材质（.uasset）只能由锁定的UE5.8编辑器创建；本目录的计划与清单不代表资源已经实现、编译或通过美术人工验收。

本次执行遵循GamePlatform（平台）、MobaCommon（MOBA可选层）、DivineBeasts（项目）的单向依赖。保留GamePlatformSurface（平台环境表面插件）的七个已声明MF（材质函数）、两个M（材质母版）及MPC（材质参数集合）固定身份。

索引应统一查看MaterialCapabilityRegistry_V1.json（能力台账机器数据）与MaterialFunctionLibraryIndex.md（人类阅读索引）。
**制作阶段调度（2026-10-13）**：`Tools/Unreal/Surface/AuthorMaterialLibrarySourcePhaseUE58.py`（领域材质函数、12项PBR材质层、Actor审核蓝图与中性QA审核母材质的串行制作入口）。默认inspect；真正UE编辑器才可apply；不执行最终编译、Cook。UI仍必须由Monolith制作。当前编辑器启动尚未解除阻断，脚本存在不等于对应二进制资产已生成。
**P6 UI函数真实制作前置清单**：`Tools/Unreal/UI/PrepareMaterialLibraryMonolithActions.py`（逐项转换7个UI函数配方的动作生成器），`Docs/Implementation/Materials/MaterialLibraryUIActionQueue_V1.json`（7项×6步＝42条可执行Monolith资源制作动作，未执行，不等于UE资产）。`AuthorMaterialLibraryStandBlueprintUE58.py`（Actor审核展示架蓝图作者）已补上ReviewMaterial和ReviewTitle字段制作逻辑，仍须真实UE资源制作。
**P8独立审核地图资源队列**：`MaterialLibraryMapActionQueue_V1.json`（Surface/VFX/Project/UI共4张开发审核地图的Monolith原生创建请求，包含存在性检查及防覆盖策略）。**注意：JSON动作清单不是.umap资源，也不证明任何蓝图编译通过。**
**UI资源制作新入口**：Tools/Unreal/UI/AuthorMaterialLibraryUIReviewUE58.py（12份真实MD_UI审核材质作者）和Tools/Unreal/UI/AuthorMaterialLibraryWidgetMonolith.py（既有C++ DemoHub父类的UI审查Widget作者）；MaterialLibraryUIWidgetSpec_V1.json（83个唯一控件、12个材质样板卡的声明式树）。当前仅新增源码，未创建实际UI母版及WBP。
**材质层工厂修复**：AuthorMaterialLibraryLayersUE58.py已改用MaterialFunctionMaterialLayerFactory和MaterialFunctionMaterialLayerBlendFactory，真实12个Layer及共享Blend仍待编辑器创建。
**资源制作门禁**：AuthorMaterialLibrarySourcePhaseUE58.py现强制锁定UE5.8、DivineBeastsWorkspace主工程、PIE未运行和资产注册表加载完成。Tests/Assets/TestMaterialLibraryDomainRecipes.py已追加检查这些接口、UI样板规格和MaterialLayer正确工厂的用例；统一测试尚未执行。

**UE5.8 编辑器安全恢复入口（2026-10-13续建）**：先运行 `Build/Game/InspectUE58EditorModuleState.ps1`（只读检查Engine/项目/指定插件`.modules`、DLL、BuildId、编辑器相关进程与历史失败日志；指定`-WriteReport`才写审计JSON）。`Build/Game/RecoverUE58EditorForMaterialLibrary.cmd`（双击入口）默认使用 `-SkipBuild`（恢复脚本自身不调用构建，且同步返回真实退出码）；`Build/Game/RecoverUE58EditorForMaterialLibrary.ps1` 在诊断需要时允许显式 `-OnlyModules`（最小Editor模块构建），不指定则使用完整Editor目标，均通过既有 `BuildFoundation.ps1` 持锁并记录真实UBT日志。启动使用 `-NoCompile`（请求编辑器不自动编译）及 `-NoLiveCoding`；注意历史启动日志即使含 `-NoCompile` 仍可能出现UE触发的UBT尝试，不能视该开关为强制无编译保证。进程PID存活不代表Monolith连接、资源创建或最终编译验收通过。


## 首批真实UE材质作者脚本（2026-10-13）

- `MaterialAuthoringBatch_20261013.json`（八项制作批次机器台账）及`MaterialAuthoringBatch_20261013.md`（中文实施与验收说明）。
- `Tools/Unreal/Surface/AuthorMaterialCoreFunctionsUE58.py`（五项表面MF材质函数作者）与`Tools/Unreal/VFX/AuthorVFXMaterialFunctionsUE58.py`（三项视觉特效MF材质函数作者）。
- `Tests/Assets/TestMaterialFunctionAuthoringScripts.py`（四组离线图拓扑、冻结路径与失败行为测试）；本轮在模型独立工作容器中的源文件镜像执行4/4通过，不是远端Runner或UE编译。
- 八项只标记“作者脚本就绪”，UE5.8真实创建、Shader、Cook、UI/Monolith和人工视觉验收均须根据实证追加；不能把新脚本路径当成真实.uasset。

