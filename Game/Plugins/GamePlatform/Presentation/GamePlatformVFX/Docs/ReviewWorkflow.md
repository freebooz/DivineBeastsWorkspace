# ReviewWorkflow（VFX人工审查流程）

正式Review Map统一命名为 L_VFXReview。当前工作树没有合法.umap，因此本轮没有文本伪造地图资产；必须通过Unreal Editor创建。

Review覆盖10种Behavior，并为17类ContentCategory选择代表性组合，不要求170种全排列。人工检查形态、时间、Attach、LWC、Bounds、Pooling、Quality、EffectType、过度闪烁和项目美术规范。

性能审查使用Niagara Debugger、Slate/Unreal Insights等真实工具记录System instances、particles、memory和perf。当前Review Map与人工Review状态：未执行。