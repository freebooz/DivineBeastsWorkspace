# UIBoundary（界面边界）

本轮不实现正式Widget（界面控件）或页面。

UI只消费 FDivineBeastsFlowViewState（项目流程视图状态）和 AllowedActions（允许动作），包括当前步骤、Busy（忙碌）、错误、角色列表、已选角色、加载摘要、连接摘要和世界分配摘要。

UI不得直接调用Backend Adapter（后端适配器）、Session内部接口或修改流程节点。按钮Disable/Debounce（禁用/防抖）只解决体验问题，真正的幂等依赖Flow Busy、OperationId、Revision及后端唯一约束。

ViewState不包含密码、Token、TransferTicket原文或服务端密钥。
