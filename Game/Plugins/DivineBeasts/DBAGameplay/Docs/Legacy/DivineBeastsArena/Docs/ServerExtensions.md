# ServerExtensions（服务器扩展）

Server Module Gate：通过。

平台GamePlatformArenaServer新增中立IGamePlatformArenaServerProjectExtension（竞技服务器项目扩展接口），DivineBeastsArenaServer通过Modular Feature注册实现；平台层不引用项目类。

项目Server职责：
- Assignment应用前解析项目Production ModeSpec。
- 校验Map/Rule/Content/HeroCatalog Revision。
- 注入项目Hero Eligibility Provider。
- GameplayLifecycleAdapter缺失时Fail Closed。

不负责：
- 通用Phase。
- Ticket加密。
- Agones。
- DB。
- MMR/奖励。
- UI/VFX。
