# WorldReadiness（世界就绪）

WorldReady 不是 PostLoadMap 事件。

项目 Loading Operation（加载操作）要求六个任务全部Ready：SessionAdmission、ExpectedWorld、ExpectedExperience、CharacterBinding、GameplayData、ProjectReadiness；同时 Session 必须处于 Admitted（已准入）。

只有 Loading 聚合 Ready + Session Admitted + 当前节点仍为 TransferWorld 时，Flow 才进入 WorldReady→InWorld。
