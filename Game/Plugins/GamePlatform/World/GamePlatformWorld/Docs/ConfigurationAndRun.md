# 配置、构建与运行

以下从真实工作树根目录执行，PowerShell 7。引擎由已有UE_ROOT环境变量指定，禁止在脚本硬编码个人盘符。每轮RunId使用新GUID，拒绝覆盖旧证据。现阶段三目标在历史空描述扫描失败，后续UE命令为已核对入口的续作命令，**本轮未运行成功**。

## 1. 离线检查与正式目标

```powershell
& ./Tests/Integration/World/TestWorldScripts.ps1
python -B -m unittest discover -s Tests/Integration/World -p 'test_*.py' -v
& ./Build/Validation/VerifyWorld.ps1 -NativeTests
& ./Build/Validation/VerifyWorld.ps1 -Targets @('Editor','Client','Server') -EngineRoot $env:UE_ROOT
```

VerifyWorld无完整运行/Cook/多PIE证据时整体返回2，不代表所选原生测试失败。已启动外部程序的失败码原样保留，必须同时读result.json.Status；外部退出2也可能是Failed。脚本不会移动历史空描述、降低UE版本或换工程。

## 2. 编译之后才生成资产

```powershell
$project = (Resolve-Path './Game/DivineBeastsArena.uproject').Path
$editor = Join-Path $env:UE_ROOT 'Engine/Binaries/Win64/UnrealEditor.exe'
$maps = (Resolve-Path './Tools/AssetTools/CreateFoundationAssets.py').Path
$world = (Resolve-Path './Tools/AssetTools/CreateWorldAssets.py').Path
& $editor $project -EnablePlugins=PythonScriptPlugin -unattended -ScriptErrorsAreFatal "-ExecutePythonScript=$maps"
& $editor $project -EnablePlugins=PythonScriptPlugin -unattended -ScriptErrorsAreFatal "-ExecutePythonScript=$world"
```

第一条默认Maps阶段，通过完整编辑器生成并保存Bootstrap和Sandbox；不是PythonScript commandlet。第二条只创建DA_FoundationRegionA、DA_FoundationRegionB、DA_FoundationWorld三个白名单定义。已有资产只验证/报告差异，不能覆盖人工修改。再次运行验证nooverwrite。任何异常先检查报告，禁止自动删除资产重试。

预期身份：GamePlatformDefinition:foundation.world@1、foundation.region_a@1、foundation.region_b@1。稳定身份来自LogicalId，不从文件名推导。地图软引用为/Game/Development/Foundation/Maps/L_FoundationSandbox.L_FoundationSandbox。

## 3. UE自动化与数据验证

```powershell
$editorCmd = Join-Path $env:UE_ROOT 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
& $editorCmd $project -unattended -NullRHI '-ExecCmds=Automation RunTests GamePlatform.World' '-TestExit=Automation Test Queue Empty'
& $editorCmd $project -unattended -run=GamePlatformWorldValidation
```

必须检查自动化导出中的具名测试、错误与跳过，不能只看进程退出。世界专用验证Commandlet负责非空世界/区域集合、地图和引用校验，并使非法/未验证资产返回非零；不能把UE原生DataValidation默认退出行为当作严格验收。该入口需WorldEditor反射成功后才能运行，本轮未执行。

## 4. FoundationWorld真实运行

```powershell
& ./Tests/Integration/World/TestWorldFoundation.ps1 -Start -EngineRoot $env:UE_ROOT `
  -DefinitionPackage '/Game/Development/Foundation/Definitions/DA_FoundationWorld' `
  -BuildResult './Saved/Validation/GamePlatformWorld/<本次成功Editor构建RunId>/result.json' `
  -Phase FullLifecycle
```

BuildResult必须换为真实成功报告，不接受本轮失败报告。入口只启用FoundationWorld；不能附加FoundationStandalone运行开关启动旧Flow。CustomConfig=FoundationStandalone仅复用开发内容配置，不是启动旧流程。FullLifecycle显式FoundationWorldExercise由项目胶水执行Sandbox→Bootstrap→Sandbox，真实新代次再次Ready才完成；默认普通FoundationWorld不自动旅行。

日志绑定当前RunId、真实PID和世界代次，进程超时由脚本处理；只停止本次所属进程。没有图形人工观察时，三维可见性独立标未执行。

开发专服需使用已构建/暂存的DivineBeastsArenaServer及相同地图/定义，显式FoundationWorldServerRole=OpenWorld、Village或MainArena，禁用FoundationWorldExercise；专服不得创建玩家UI。当前没有专服世界运行证据，不以客户端日志代替。

## 5. Session、WP、Cook与Stage

```powershell
& ./Tests/Integration/World/TestWorldSession.ps1 -Start -EngineRoot $env:UE_ROOT
& ./Tests/Integration/World/TestWorldPartition.ps1
& ./Build/Game/CookFoundation.ps1 -Cook -Target Client -EngineRoot $env:UE_ROOT
& ./Build/Game/CookFoundation.ps1 -Cook -Target Server -EngineRoot $env:UE_ROOT
```

Session入口当前因公开快照/真实准入缺失返回未执行2。Partition入口默认不启动；WP地图及项目运行适配未具备，不能传Foundation非WP地图冒充验证。本轮未生成WP测试地图。

CookFoundation以独占新Cook/Stage目录执行，不删除旧产物；复用显式FoundationStandalone开发内容配置，正式DefaultGame.ini仍排除整个Foundation目录。必须另行审计服务器产物与发行排除，配置文本不是Cook通过证据。
