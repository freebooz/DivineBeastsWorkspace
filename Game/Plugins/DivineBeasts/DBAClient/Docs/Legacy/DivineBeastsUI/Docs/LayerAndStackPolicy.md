# LayerAndStackPolicy（界面层与页面栈策略）

项目复用 GamePlatformUI（游戏平台UI）已有CommonUI层栈，不创建第二套Root或Stack。

使用的层：

- HUD：OpenWorld/Village/Arena常驻HUD。
- Screen/Menu：登录、角色、匹配、赛后等主页面。
- Modal：错误/重连、Match Found Ready等需要阻断下层交互的页面。
- System：Scoreboard/System Menu。
- Notification：Toast，不抢焦点。
- Loading：关键加载/切服期间锁输入。
- Debug：平台已有开发层；Shipping（正式发布）必须剥离调试内容。

所有页面关闭、Travel（切服）和LocalPlayer销毁均由平台UI Manager生命周期统一处理。Dedicated Server不参与任何页面栈逻辑。
