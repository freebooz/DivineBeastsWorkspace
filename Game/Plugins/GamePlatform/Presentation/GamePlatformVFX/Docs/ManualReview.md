# ManualReview（人工审查）

当前状态：未执行。

原因：本轮没有伪造L_VFXReview.umap，也没有创建虚假Niagara System、Niagara Effect Type或测试.uasset；Runner同时没有UE5.8工具链和Android目标设备。

人工审查必须覆盖：10种Behavior代表资源、17类Category代表组合、Attach/Socket、Composite取消、预测去重、LWC、Fixed Bounds、Effect Type、Local Player关键反馈、PC/Android质量、1v1/5v5/OpenWorld/Village性能、Niagara Debugger数据、Client/Server Cook清单。

只有取得真实Editor/设备/工件证据后才可将对应项从“未执行”更新为“通过/失败”。