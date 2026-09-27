# API（接口）

Runtime主要公共契约：

- FDivineBeastsPresentationProjectContext（项目表现上下文）。
- FDivineBeastsPresentationProjectCatalog（项目默认表现目录）。
- FDivineBeastsPresentationContentPackFragment（内容包表现片段）。
- FDivineBeastsWorldInteractionPresentationFact（世界交互表现事实）。
- FDivineBeastsVillageFeedbackPresentationFact（新手村反馈表现事实）。
- DBA.Presentation.World.Interaction.Committed / DBA.Presentation.Village.Guidance.Ready（项目表现语义标签）。

Client主要接口：

- UpdateProjectContext（更新项目上下文）。
- ActivateContentPack / DeactivateContentPack（激活/停用内容包）。
- SubmitWorldInteractionFact / SubmitVillageFeedbackFact（提交非MOBA事实）。
- RequestLogicalPreload / CancelLogicalPreload（逻辑预加载协调）。
- ResetForAccountSwitch（账号切换清理）。

接口只传稳定逻辑ID和中立请求，不传任意资源路径、Token、Ticket或Backend DTO。
