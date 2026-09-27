# ConfigurationAndRun（配置与运行）

源码静态验证：

powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/Characters/TestDivineBeastsCharacters.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/Characters/TestCharacterReplication.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/Characters/TestCharacterApplicationFlowProvider.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/Characters/TestCharacterServerCook.ps1

综合验证：
Build/Validation/VerifyDivineBeastsCharacters.ps1

UE Editor合法生成12个Definition：
Build/Validation/GenerateDivineBeastsHeroDefinitions.ps1

当前Runner没有UE_ROOT/UnrealEditor-Cmd，因此资产生成、UE Automation、Client/Server Build和Cook必须保持“未执行”。
