# GamePlatformSurface 性能与服务器裁剪

Surface运行时不使用 Tick；上游环境事实变化时才提交状态，相同状态去重。全局天气参数通过每世界一个 MPC Instance 更新，避免遍历全部 Actor 并逐个创建 Dynamic Material Instance。

局部脚印、污渍、交互区域等不应滥用全局 MPC，应根据场景采用局部材质参数、Decal（贴花）、Runtime Virtual Texture（运行时虚拟纹理）或专门领域机制。

平台母材质应通过 Static Switch（静态开关）裁剪未启用 Snow／Moss／Wetness／Puddle；远景和低档位使用 Lite 变体。三平面、程序噪声、WPO、多层法线混合等只有真实 ProfileGPU／Shader Complexity（着色器复杂度）测量后才能决定默认启用范围，本插件不伪造毫秒或显存预算。

`GamePlatformSurfaceClient`为 ClientOnly，`GamePlatformSurfaceEditor`为 Editor；`DivineBeastsArenaServer.Target.cs`不得启用本插件。服务器需要“雪地区域”“泥地区域”或真实水体时，应读取服务器安全 World／Gameplay Definition，而不是 Material、Texture、MPC 或客户端 Surface 状态。

最终发布必须审计 Server Cook／Stage：纯 Surface Material／Texture／Material Function 和客户端模块二进制不应进入 Dedicated Server 产物。仅有 TargetAllowList 声明不能替代产物审计。

标准 MPC 使用软引用，仅在世界首次需要绑定时加载。Surface不建立第二套 Asset Manager（资产管理器）。
