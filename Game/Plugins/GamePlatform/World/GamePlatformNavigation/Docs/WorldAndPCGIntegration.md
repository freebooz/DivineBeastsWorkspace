# WorldAndPCGIntegration（世界与PCG集成）

`UGamePlatformNavigationWorldSubsystem`绑定 UWorld 生命周期；初始化生成新的 WorldGeneration，Deinitialize 逻辑取消异步请求、撤销 Invoker、清理 Profile/Filter Registry，并推进 Generation，使旧回调失效。

服务器模块依赖 `GamePlatformWorld（平台世界插件）`作为未来 World/Region/Streaming 集成边界，但本轮不维护第二份 World 目录，也不实现跨服务器路径。

Navigation 不依赖 `GamePlatformPCG（程序化生成插件）`执行模块。装饰性 PCG 不应改变服务器可通行性；碰撞/导航相关 PCG 必须服务器权威或一致烘焙。

当前没有 PCG 导航测试地图，也没有 World Partition Navigation 地图；PCG障碍参与 NavMesh、Cell流入流出和请求清理均为未执行。
