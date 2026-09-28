# LayerStack（界面层栈）

统一层模型：HUD（常驻界面）、WorldProjection（世界投影）、Feedback（高频反馈）、Screen（普通页面）、Notification（屏幕通知）、Modal（模态页面）、System（系统页面）、Loading（加载）、Debug（调试）。

Screen/Modal/System/Loading/Debug 使用 `UCommonActivatableWidgetStack（CommonUI可激活控件栈）`，栈顶负责激活和输入；HUD/WorldProjection/Feedback/Notification 使用 `UOverlay（覆盖容器）`，不成为新的输入路由节点。WorldProjection 负责名称板/Marker（标记），Feedback 负责伤害飘字/命中等高频短生命周期表现。

Modal（模态）应由页面自身 InputMode 配置阻断下层交互；Notification（通知）不主动抢焦点。Loading（加载）作为独立高层，具体 Root Layout 蓝图应保证视觉 ZOrder（层级顺序）。

Debug（调试）资源必须在 Shipping（正式发布）中排除，当前源码仅提供层位置，不自动创建调试页面。
