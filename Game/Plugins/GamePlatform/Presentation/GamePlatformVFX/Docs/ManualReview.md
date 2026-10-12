# ManualReview（人工审查）

当前状态（2026-10-12库存复核）：仓库已存在平台通用VFX和项目表现的真实UE二进制资源文件，包括Niagara相关System、EffectType及Definition；早期“仓库没有这些资产”的结论已过时。源码、资源库存、引擎重载和人工画面是不同证据。

标准 `L_VFXReview.umap` 仍未交付；已有BirdFlock、Fauna、Weather、Snow和FallingFoliage开发审核图不等同统一Review Map。统一场景、步骤、预期与阻塞记录见项目 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json` 的vfx-review条目。

历史定向模块编译仅证明当时C++/UHT/链接合同；2026-10-12诊断修改需本次重新构建与Automation，结果以本轮ExecutionProgress为准。任何编译结果都不证明Niagara内容质量或目标设备性能。

VFX Lib 迁移的人工审核必须额外确认：技术母版不残留 FrostMage/PetalBloom/生肖命名；SourceArt 导入后的纹理压缩、Alpha、Mipmap 与采样方式正确；FXM 模块封装后的 HLSL 输出与来源审核画面在运动节奏上等价，但视觉主题由项目内容重做。

人工审核必须覆盖：

- Generic Niagara + Composite 的代表性资源；
- Attached默认Socket、Beam Source/Target、Area Radius；
- Predicted/Confirmed/Corrected/Cancelled；
- OnSystemFinished回收与MaxLifetime；
- Composite取消和Hard Budget；
- LWC、Fixed Bounds、Effect Type、Pooling；
- LocalPlayer关键反馈与World Travel；
- PC/Android质量；
- 1v1/5v5/OpenWorld/Village Niagara Debugger / Unreal Insights；
- Client/Server Cook清单。

只有真实 Editor/设备/工件证据存在后，相关项才可从“未执行”更新为“通过/失败”。