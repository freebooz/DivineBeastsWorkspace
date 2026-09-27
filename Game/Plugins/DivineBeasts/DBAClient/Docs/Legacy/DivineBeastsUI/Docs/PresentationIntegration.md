# PresentationIntegration（表现集成）

DivineBeastsPresentation（项目表现插件）与DivineBeastsUI（项目UI插件）职责分离：

- Presentation适合Toast semantic（短提示语义）、feedback semantic（反馈语义）、damage number等瞬时表现事件。
- UI负责Login、Character、Loading、World HUD、Arena HUD、PostMatch等业务页面状态机。

DivineBeastsUIClient当前不硬依赖DivineBeastsPresentationClient。

当前项目Presentation Catalog尚无ProviderChannel=UI的真实已批准Semantic/Definition，DBAUIPack_Core也不存在，因此没有注册项目UI Presentation Provider。该组合状态为“未执行”，而不是通过。

未来如出现正式UI semantic，应在DBAClient组合层把GamePlatformPresentation中立请求适配到平台Toast/Dialog入口，不能让Presentation替代业务View State。
