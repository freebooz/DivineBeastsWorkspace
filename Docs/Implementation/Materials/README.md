# 神兽联盟通用材质函数库建设（2026-10-13）

本目录承担材质能力归类、UE资产实际审计、三层插件所有权、参数合同、表现组合、质量与验收证据。真实材质（.uasset）只能由锁定的UE5.8编辑器创建；本目录的计划与清单不代表资源已经实现、编译或通过美术人工验收。

本次执行遵循GamePlatform（平台）、MobaCommon（MOBA可选层）、DivineBeasts（项目）的单向依赖。保留GamePlatformSurface（平台环境表面插件）的七个已声明MF（材质函数）、两个M（材质母版）及MPC（材质参数集合）固定身份。

索引应统一查看MaterialCapabilityRegistry_V1.json（能力台账机器数据）与MaterialFunctionLibraryIndex.md（人类阅读索引）。

## 首批真实UE材质作者脚本（2026-10-13）

- `MaterialAuthoringBatch_20261013.json`（八项制作批次机器台账）及`MaterialAuthoringBatch_20261013.md`（中文实施与验收说明）。
- `Tools/Unreal/Surface/AuthorMaterialCoreFunctionsUE58.py`（五项表面MF材质函数作者）与`Tools/Unreal/VFX/AuthorVFXMaterialFunctionsUE58.py`（三项视觉特效MF材质函数作者）。
- `Tests/Assets/TestMaterialFunctionAuthoringScripts.py`（四组离线图拓扑、冻结路径与失败行为测试）；本轮在模型独立工作容器中的源文件镜像执行4/4通过，不是远端Runner或UE编译。
- 八项只标记“作者脚本就绪”，UE5.8真实创建、Shader、Cook、UI/Monolith和人工视觉验收均须根据实证追加；不能把新脚本路径当成真实.uasset。

