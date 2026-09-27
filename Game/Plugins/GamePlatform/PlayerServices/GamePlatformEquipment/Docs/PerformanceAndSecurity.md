# PerformanceAndSecurity（性能与安全）

装备槽数量少、变化频率低，当前使用普通 replicated snapshot（复制快照），而不是为尚未验证的规模提前引入 FastArray（快速数组）。

Client Visual 只在 Public Snapshot/Avatar 变化时异步加载，不每帧请求资产；旧请求通过 FStreamableHandle 取消，并由 VisualRequestGeneration/AvatarGeneration 双重过滤。

安全上，客户端不能提交属性、GameplayEffect、Ability Class、Mesh、Material 或 SocketName。后端最终验证 Inventory 所有权、两个 Revision 与 Slot。真实 Player↔Character 和 Server↔Player 控制面绑定验证仍缺失。