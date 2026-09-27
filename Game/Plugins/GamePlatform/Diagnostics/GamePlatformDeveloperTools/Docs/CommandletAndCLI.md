# CommandletAndCLI（命令行验证）

资产验证使用 UE 官方 Data Validation：UnrealEditor-Cmd.exe DivineBeastsArena.uproject -run=DataValidation。

Build/Validation/ValidateData.ps1 负责 UE_ROOT 检查、路径引用、超时和真实退出码传播。

自定义 UGamePlatformValidationCommandlet 只覆盖标准 Data Validation 不适合的规则，支持 Mode=Architecture、Cook、Release、Audit。

Mode=Data 会拒绝执行并提示使用官方 DataValidation，避免重复平行资产验证框架。Commandlet 不依赖 LocalPlayer、Game World 或 Modal Dialog。