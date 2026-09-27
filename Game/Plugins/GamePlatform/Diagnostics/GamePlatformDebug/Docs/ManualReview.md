# ManualReview（人工审查）

状态：未执行。AI 未代签。

人工审查至少覆盖：
- Development/Test/Shipping Client Build（开发/测试/正式客户端构建）。
- Development/Test/Shipping Server Build（开发/测试/正式服务器构建）。
- Client/Server Cook（客户端/服务器烘焙）。
- PIE 1 Client、PIE 2 Clients（单/双客户端编辑器运行）。
- Separate Dedicated Server（独立专用服务器）。
- Development Server + 2 Clients（开发服务器+双客户端）。
- Multi-PIE（多编辑器实例）目标隔离。
- Target Destroy / World Travel（目标销毁/世界切换）。
- Gameplay Debugger（玩法调试器）8个 GP.* 分类。
- Client/Server View（客户端/服务器视角）真实性。
- Sensitive Filter（敏感过滤）。
- Unreal Insights / Networking Insights（虚幻分析器/网络分析器）。
- Shipping 中无 gp.Debug.*、无 Client Panel（客户端面板）、无自定义 Debug RPC（调试RPC）、无 Debug Asset（调试资产）。
- Manual/1/5/10Hz（手动/刷新频率）开销。

当前结构测试通过不等价于上述人工审查通过。