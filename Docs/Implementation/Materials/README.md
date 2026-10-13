# 神兽联盟通用材质函数库建设（2026-10-13）
> **最新制作进度与真实验收边界**：`MaterialLibrarySourceStatus_20261013.md`（源码、蓝图资源优先制作清单）。首批8个MF及第二批18个MF已由UE真实生成、隔离Shader验证；另5个UI函数已保存但UI Shader未验。剩余60个独立MF、12个材质层、7个UI函数配方及独立演示蓝图/地图已完成制作代码，但**真实二进制资产尚未生成**。最终全目标编译、双Cook、GPU与人工签收均推迟到资源制作完成后，不能以作者脚本当作交付。
> **执行任务索引**：`MaterialLibraryResourceWorkQueue_20261013.json`（先源码/蓝图/资源，最后统一测试的16项阶段任务）；`MaterialFunctionLibraryIndex.md`（201项逐条实时实施状态），机器真源`MaterialCapabilityRegistry_V1.json`（归属、完成度及建议UE路径）。Cook排除已写入受管政策真源及DefaultGame.ini，尚未生成最终Server Profile或实际执行Cook。



本目录承担材质能力归类、UE资产实际审计、三层插件所有权、参数合同、表现组合、质量与验收证据。真实材质（.uasset）只能由锁定的UE5.8编辑器创建；本目录的计划与清单不代表资源已经实现、编译或通过美术人工验收。

本次执行遵循GamePlatform（平台）、MobaCommon（MOBA可选层）、DivineBeasts（项目）的单向依赖。保留GamePlatformSurface（平台环境表面插件）的七个已声明MF（材质函数）、两个M（材质母版）及MPC（材质参数集合）固定身份。

索引应统一查看MaterialCapabilityRegistry_V1.json（能力台账机器数据）与MaterialFunctionLibraryIndex.md（人类阅读索引）。

## 首批真实UE材质作者脚本（2026-10-13）

- `MaterialAuthoringBatch_20261013.json`（八项制作批次机器台账）及`MaterialAuthoringBatch_20261013.md`（中文实施与验收说明）。
- `Tools/Unreal/Surface/AuthorMaterialCoreFunctionsUE58.py`（五项表面MF材质函数作者）与`Tools/Unreal/VFX/AuthorVFXMaterialFunctionsUE58.py`（三项视觉特效MF材质函数作者）。
- `Tests/Assets/TestMaterialFunctionAuthoringScripts.py`（四组离线图拓扑、冻结路径与失败行为测试）；本轮在模型独立工作容器中的源文件镜像执行4/4通过，不是远端Runner或UE编译。
- 八项只标记“作者脚本就绪”，UE5.8真实创建、Shader、Cook、UI/Monolith和人工视觉验收均须根据实证追加；不能把新脚本路径当成真实.uasset。

