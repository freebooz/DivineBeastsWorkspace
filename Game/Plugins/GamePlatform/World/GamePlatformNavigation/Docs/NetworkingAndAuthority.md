# NetworkingAndAuthority（网络与权威）

`GamePlatformNavigationServer`是 ServerOnly（仅服务器）模块，服务器 AI 的权威 Path Query、Profile、Filter、NavData、Goal 和 World 都在服务器选择。

共享 Request 虽允许 Authority=Advisory 用于未来客户端预览，但当前没有客户端导航服务实现；客户端 Contract（契约）不能替换服务器 AI 路径、Combat、Interaction 或胜负判定。

公共请求传稳定 AgentProfileId/FilterId，不传任意 Filter Class、NavArea Class、NavData Actor 或服务器内部指针。

Client Target 是否最终不链接 NavigationServer、Server Target 是否包含 NavigationServer，需要真实三目标 Build/Cook 产物验证；静态模块宿主只能作为结构证据。
