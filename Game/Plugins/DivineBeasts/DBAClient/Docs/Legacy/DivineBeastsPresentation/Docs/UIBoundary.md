# UIBoundary（界面边界）

UI执行与页面体系归GamePlatformUI（游戏平台UI）和后续DivineBeastsUI（神兽联盟项目UI）。

DivineBeastsPresentation不创建Widget、不维护HUD/ViewModel业务、不处理按钮或页面导航。它只能通过中立ProviderChannel + DefinitionId表达“需要何种表现”。

DBAUIPack_Core当前只有规划引用，没有真实插件/资产，因此UI内容包接入状态为“未执行”。
