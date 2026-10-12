# ReviewWorkflow（VFX人工审查流程）

正式Review Map统一命名为 L_VFXReview。当前已有多个真实专项开发审核图，但尚无统一L_VFXReview；后续在现有主工程的开发目录通过Unreal Editor创建，不增加第二工程。专项图与统一图的资源加载、视觉及性能结果各自留证。

Review覆盖10种Behavior，并为17类ContentCategory选择代表性组合，不要求170种全排列。人工检查形态、时间、Attach、LWC、Bounds、Pooling、Quality、EffectType、过度闪烁和项目美术规范。

性能审查使用Niagara Debugger、Slate/Unreal Insights等真实工具记录System instances、particles、memory和perf。当前Review Map与人工Review状态：未执行。