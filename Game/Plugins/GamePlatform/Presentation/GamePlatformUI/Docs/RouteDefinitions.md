# RouteDefinitions（UI路由定义）

UGamePlatformUIRouteDefinition（游戏平台UI路由定义）只描述 RouteId（路由编号）、ScreenId（页面编号）、Layer（层）、Transition（过渡）、Required/Blocked Tags（所需/阻止标签）、FallbackRouteId（降级路由）和 BackRouteId（返回路由）。它不是 ApplicationFlow（应用流程）或任何游戏业务状态机。

当前注册门禁要求：

- RouteId和ScreenId不能为空。
- 目标Screen必须已经注册。
- Route Layer必须与目标Screen Definition的Layer一致。
- Route只能指向 Screen/Modal/System/Loading/Debug 可激活层。
- FallbackRouteId/BackRouteId不能指向自身。
- RequiredTags与BlockedTags不能重叠。
- Shipping构建禁止Debug Route。
- fallback/back有向图形成循环时拒绝新增Route。
- Route被其它Route的FallbackRouteId/BackRouteId引用时拒绝注销，避免留下悬空引用；应先解除引用再注销。

GetBackRouteId（获取返回路由）只读取显式BackRoute元数据，不把“登录后去角色页”“比赛结束回开放世界”等业务流程编码到平台层。

Route条件不满足时只允许进入显式Fallback；项目层应由Composition Root（组合根）根据权威View State（视图状态）决定何时调用哪个Route。

