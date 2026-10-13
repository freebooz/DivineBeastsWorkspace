# 通用材质函数首批八项制作与验证批次（2026-10-13）

## 本批真实状态

已在 `DivineBeastsWorkspace`（神兽联盟工作空间）建立5项Surface（平台环境表面）和3项VFX（平台视觉特效）的 UE5.8 Material Function（材质函数）作者脚本。它们不覆盖已有七项稳定MF（材质函数）和两个Surface母版；不存在独立的平行材质插件。源代码已通过脱离UE的基本Python语法、只读inspect入口以及仿真节点图结构测试（4组，均通过），**没有把作者脚本误记为已保存的真实.uasset资产**。

| 编号 | 材质函数目标（中文能力） | 所属插件 | 节点实现配方 | 实际状态 |
|---|---|---|---|---|
| MAT-01-09 | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_HeightBlendMask`（高度混合遮罩） | GamePlatformSurface（平台环境表面） | `height`（高度混合遮罩） | 作者脚本完成；UE待执行 |
| MAT-01-07 | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_MacroMicroBlend`（宏微观纹理混合） | GamePlatformSurface（平台环境表面） | `macro`（宏微观纹理混合） | 作者脚本完成；UE待执行 |
| MAT-04-10 | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeBiomeMask`（地形生态区域遮罩） | GamePlatformSurface（平台环境表面） | `biome`（地形生态区域遮罩） | 作者脚本完成；UE待执行 |
| MAT-04-06 | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeDistanceDetail`（远近景颜色与细节衰减） | GamePlatformSurface（平台环境表面） | `distance`（远近景颜色与细节衰减） | 作者脚本完成；UE待执行 |
| MAT-02-05 | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_FrostSurface`（霜层基础色、粗糙度和法线） | GamePlatformSurface（平台环境表面） | `frost`（霜层基础色、粗糙度和法线） | 作者脚本完成；UE待执行 |
| MAT-09-01 | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_FlowUV`（二维UV流动） | GamePlatformVFX（平台视觉特效） | `uvflow`（二维UV流动） | 作者脚本完成；UE待执行 |
| MAT-09-09 | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_Dissolve`（透明溶解和边缘遮罩） | GamePlatformVFX（平台视觉特效） | `dissolve`（透明溶解和边缘遮罩） | 作者脚本完成；UE待执行 |
| MAT-09-14 | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_RadialEnergyGradient`（径向能量遮罩） | GamePlatformVFX（平台视觉特效） | `radial`（径向能量遮罩） | 作者脚本完成；UE待执行 |

## 脚本及执行方式

- `Tools/Unreal/Surface/AuthorMaterialCoreFunctionsUE58.py`（平台通用表面五函数作者）。
- `Tools/Unreal/VFX/AuthorVFXMaterialFunctionsUE58.py`（平台VFX三函数作者）。
- `Tests/Assets/TestMaterialFunctionAuthoringScripts.py`（不加载UE的节点图结构测试）。
- `Docs/Implementation/Materials/MaterialAuthoringBatch_20261013.json`（本批八项目标与逐项证据）。

在外部普通Python运行 `python Tools/Unreal/Surface/AuthorMaterialCoreFunctionsUE58.py --mode inspect`（仅打印预定资产、无写入）和 `python Tools/Unreal/VFX/AuthorVFXMaterialFunctionsUE58.py --mode inspect`（仅打印预定资产）。在**经确认的正式UE5.8编辑器**内使用相同脚本的 `--mode apply`（生成真实UE资产）及 `--mode verify`（回读核查）执行，不得从普通Python伪造.uasset。

引擎执行顺序：先完成 `InventoryMaterialLibraryUE58.py --mode scan`（只读资产注册表审计），审核所有目标的引用与磁盘占用，再分别执行Surface与VFX作者、保存回读并编译真实调用方材质。任何名称冲突、类别错误或节点不足必须明确失败，不能删除/覆盖旧资源。UI（用户界面）材质制作按原项目强制要求由Monolith MCP完成，不使用本作者脚本绕开UI资产规则。

## 与P0—P10的对应关系

P0真实资产扫描：待UE启动；P1三层归属及201项台账：已完成文档初步归集；P2表面5函数：代码就绪、尚未生成UE资产；P5 VFX3函数：代码就绪、尚未生成UE资产；P3/P4/P6/P7/P8/P9真实资源、Shader、Niagara、蓝图、演示、性能及Cook测试：尚未通过；P10人工验收：待执行。

编辑器作业 `detached_0TgmAmK0jtV4sUQoeomvfrtArpA6eVC4bmZMMXXtrJE`（UE5.8编辑器启动）最后观察为 `agent_queued`（任务在队列，启动命令尚未执行）。其他会话运行中的进程不得因本批任务自行中止。只有编辑器实际运行且完成上述测试后才能修改资源已交付状态。

> 本批JSON是批次进度真源；201项全量能力及其初步身份仍在 `MaterialCapabilityRegistry_V1.json`（201项机器索引）和 `MaterialFunctionLibraryIndex.md`（中文能力索引）。
