# CompositionRootIntegration（组合根集成）

DBAClient（神兽联盟客户端组合插件）是当前UI Composition Root（组合根）。

UDBAUICompositionSubsystem 同时读取：

- DivineBeastsApplicationFlow View State（应用流程视图状态）
- GamePlatformLoading Snapshot（平台加载快照）
- DivineBeastsArenaClient项目模式目录
- Replicated Arena GameState/PlayerState（复制的竞技状态）
- GamePlatformCombat只读Health/Shield（战斗生命/护盾）
- GamePlatformInteraction Focus（交互焦点）

然后实现 IDivineBeastsUIQuerySource 和 IDivineBeastsUICommandPort。

未存在真实Owner提交端口的功能不会在组合层造假：

- Matchmaking transport（匹配传输）：未执行，返回Unavailable。
- Arena Hero Selection submit（竞技选人提交）：未执行，返回Unavailable。
- Training Reset（训练重置）：未执行，返回Unavailable。

这样既保留页面和ViewModel工程接口，也不会让UI成为玩法真源。
