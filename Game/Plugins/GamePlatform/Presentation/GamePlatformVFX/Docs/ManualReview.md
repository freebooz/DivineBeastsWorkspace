# ManualReview（人工审查）

当前状态：真实 VFX 二进制资产/Review Map 人工审核尚未执行。

本轮 Runner 已可访问 UE5.8 构建工具链，但仓库仍没有合法 Niagara System、Effect Type、VFX Definition 或 `L_VFXReview.umap`，因此不能伪造美术运行验收。

当前代码层证据：`GamePlatformVFXClient` 与 `GamePlatformVFXEditor` 已完成 UE5.8 Editor 定向模块编译；这只能证明当前 C++/UHT/链接合同成立，不证明真实 Niagara 内容质量或目标设备性能。

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