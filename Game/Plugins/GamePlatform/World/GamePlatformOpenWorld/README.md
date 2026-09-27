# GamePlatformOpenWorld（游戏平台开放世界上层策略候选插件）

当前状态：**条件保留／未实现**。现有 Runtime 与 ServerOnly 模块只有注册入口，真实活动源码中没有外部消费者。

开放世界基础能力已经分别归属：
- GamePlatformWorld：世界／区域／流送／就绪。
- GamePlatformPCG：程序化生成与Bake合同。
- GamePlatformNavigation：导航。
- GamePlatformInteraction：交互。
- GamePlatformAI：AI。

因此本插件不得变成“OpenWorld万能Manager”。只有未来形成跨项目通用且独立的 Dynamic World Event、Zone Activity、Population Scheduler 等明确职责时才应实施；否则应在正式基线变更后退休。

现行规则：
- [游戏端核心要求](../../../../../Docs/Architecture/游戏端核心要求.md)
- [插件系统P0收敛审计](../../../../../Docs/Architecture/游戏端插件系统P0收敛审计.md)
- [插件开发规范](../../../插件开发规范.md)

当前不得作为开放世界已经完成的证明。
