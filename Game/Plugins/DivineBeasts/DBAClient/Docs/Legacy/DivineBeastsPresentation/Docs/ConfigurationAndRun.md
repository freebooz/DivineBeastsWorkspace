# ConfigurationAndRun（配置与运行）

静态验证：

powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsPresentation/TestProjectPresentation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsPresentation/TestPresentationCatalog.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsPresentation/TestPresentationContentPacks.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsPresentation/TestPresentationIntegration.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsPresentation/TestPresentationServerCook.ps1

综合验证：
Build/Validation/VerifyDivineBeastsPresentation.ps1。

真实UE Automation、Editor/Client/Server Build、Cook、Multi-PIE和5v5压力必须在锁定UE5.8环境执行。
