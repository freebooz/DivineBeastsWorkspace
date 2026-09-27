# ConfigurationAndRun（配置与运行）

静态专项验证：

- Tests/Integration/UI/TestDivineBeastsUI.ps1
- Tests/Integration/UI/TestUIFlow.ps1
- Tests/Integration/UI/TestUIInputFocus.ps1
- Tests/Integration/UI/TestUIArena.ps1
- Tests/Integration/UI/TestUICook.ps1

综合验证：

- Build/Validation/VerifyDivineBeastsUI.ps1

平台UI回归：

- Build/Validation/VerifyGamePlatformUI.ps1

真实UE运行前还必须由Unreal Editor创建：

- CommonUI Root Layout Blueprint（根布局蓝图）
- WBP_UI_* Screen/HUD/Dialog/Toast
- 持久化Screen Definition资产或确认源码瞬时Definition策略
- DBAUIPack_Core等正式UI内容包

当前Runner无UE_ROOT/UnrealEditor-Cmd.exe，因此UE Automation、Editor/Client/Server Build、Client/Server Cook和Multi-PIE均不能标通过。
