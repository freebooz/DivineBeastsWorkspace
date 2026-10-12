#requires -Version 5.1
<#
.SYNOPSIS
UE5.8 GoldLevel（PCG金标准关卡）空间快照探针与受控真实生成预览。
.DESCRIPTION
禁止与其它UE编辑器/UBT构建并发；先核对PCGEditor DLL编译时间晚于本阶段C++变更。
默认SpatialProbe仅只读核对空间快照；显式GeneratePreview才生成并清理测试实例。
两阶段均不保存地图、不触及正式Village、不做Cook；必须检查UE返回码和对应成功日志。
#>
[CmdletBinding()]
param([string]$EngineRoot = $env:UE_ROOT,
    [ValidateSet('SpatialProbe','GeneratePreview')][string]$Stage = 'SpatialProbe')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$game=Join-Path $root 'Game/DivineBeastsArena.uproject'
$map=Join-Path $root 'Game/Content/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1.umap'
$module=Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Binaries/Win64/UnrealEditor-GamePlatformPCGEditor.dll'
$source=Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldMapAuthoring.cpp'
$commandlet=Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldAssetsCommandlet.cpp'
$validator=Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Validators/GamePlatformPCGWorldValidator.cpp'
$pcgRoot=Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG'
$runtimeModule=Join-Path $pcgRoot 'Binaries/Win64/UnrealEditor-GamePlatformPCG.dll'
$directory=Join-Path $root ('Saved/Validation/GamePlatformPCG/SpatialProbes/'+[guid]::NewGuid().ToString())
New-Item -Path $directory -ItemType Directory -Force |Out-Null
$stdout=Join-Path $directory ('UE_Editor_'+$Stage+'.stdout.log')
$stderr=Join-Path $directory ('UE_Editor_'+$Stage+'.stderr.log')
try {
    if(!$EngineRoot){throw '必须指定UE_ROOT（UE5.8引擎根目录）或-EngineRoot。'}
    $engine=(Resolve-Path -LiteralPath $EngineRoot).Path
    $version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8|ConvertFrom-Json
    if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw '仅支持已锁定的UE5.8。'}
    $editor=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    foreach($file in @($game,$map,$module,$source,$commandlet,$validator,$editor)){
        if(!(Test-Path -LiteralPath $file)){throw ('PCG真实地图或模块缺失：'+$file)}
    }
    # 不能只看GoldMap等三个文件：Seed、P7桥规则和底层Mask也会影响真实生成结果。
    # 分别核查运行与Editor两模块的全部源码，拒绝任何比对应DLL新的cpp/h，以免旧模块伪装新验收。
    foreach($check in @(
        @{Name='GamePlatformPCG';Binary=$runtimeModule;Source=(Join-Path $pcgRoot 'Source/GamePlatformPCG')},
        @{Name='GamePlatformPCGEditor';Binary=$module;Source=(Join-Path $pcgRoot 'Source/GamePlatformPCGEditor')}
    )){
        if(!(Test-Path -LiteralPath $check.Binary) -or !(Test-Path -LiteralPath $check.Source)){
            throw ('PCG运行或编辑器模块目录/二进制缺失：'+$check.Name)
        }
        $compiled=(Get-Item -LiteralPath $check.Binary).LastWriteTimeUtc
        $stale=Get-ChildItem -LiteralPath $check.Source -Recurse -File |
            Where-Object { $_.Extension -in @('.cpp','.h') -and $_.LastWriteTimeUtc -gt $compiled } |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if($stale){
            throw ('PCG '+$check.Name+' DLL早于源码，必须正式UE5.8重新编译链接：'+$stale.FullName)
        }
    }
    if(@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count){
        throw '另一个UE编辑器正在运行，禁止并行载入同一项目地图。'
    }
    $ubt=@(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" |
        Where-Object { $_.CommandLine -match 'UnrealBuildTool' })
    if($ubt.Count){throw 'UE构建正在使用共享中间文件，拒绝争用。'}
    # 双阶段都不得更改原关卡和三份已审批生成图；前后文件哈希作为独立证据。
    $protected=@($map)
    foreach($name in @('Canopy','Rock','Crop')){
        $protected+=Join-Path $root ('Game/Content/Development/Foundation/PCG/Realized/PCG_Gold_'+$name+'.uasset')
    }
    $hashes=@{}
    foreach($asset in $protected){
        if(!(Test-Path -LiteralPath $asset)){throw ('真实Gold生成图资源缺失：'+$asset)}
        $hashes[$asset]=(Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash
    }
    $disable=@('Monolith','DBAClient','DBAArena','GamePlatformSurface',
        'DBAUIPack_Core','DBAFrontEndPack','DBAContentPack_Common',
        'DBAHeroPack_Rat','DBAHeroPack_Ox','DBAHeroPack_Tiger','DBAHeroPack_Rabbit',
        'DBAHeroPack_Dragon','DBAHeroPack_Snake','DBAHeroPack_Horse','DBAHeroPack_Goat',
        'DBAHeroPack_Monkey','DBAHeroPack_Rooster','DBAHeroPack_Dog','DBAHeroPack_Boar')
    $arguments=@($game,'-run=GamePlatformPCGGoldAssets',('-Stage='+$Stage),
        "-DisablePlugins=$($disable -join ',')",
        '-EnablePlugins=CommonUI,DBAGameplay,DBAWorlds,DBAWorldPack_Village',
        '-NoCompile','-unattended','-nop4','-nosplash','-nosound','-nullrhi','-stdout')
    # Editor启动可能停在引擎插件初始化。最多等待420秒，仅允许终止本脚本创建的Pid。
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if(!$process.WaitForExit(420000)){
        $process.Kill()
        [void]$process.WaitForExit(5000)
        throw ('本次独立UE '+$Stage+'执行超过420秒，已终止本脚本自己的进程；不影响其他编辑器。')
    }
    $process.Refresh()
    foreach($asset in $protected){
        $newHash=(Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash
        if($newHash -ne $hashes[$asset]){
            throw ('金标准独立Editor探针意外修改了已批准的真实地图/图资产，拒绝验收：'+$asset)
        }
    }
    Write-Output ($Stage+'_UE_EXIT='+$process.ExitCode)
    $marker=if($Stage -eq 'GeneratePreview'){
        'GP_PCG_GENERATE_PREVIEW_PASS'
    }else{
        'GP_PCG_SPATIAL_PROBE_PASS'
    }
    $matched=(Test-Path $stdout) -and [bool](Select-String -LiteralPath $stdout -Pattern $marker -SimpleMatch -Quiet)
    if($process.ExitCode -ne 0 -or !$matched){
        $tail=@(Get-Content $stdout -Encoding UTF8 -Tail 22 -ErrorAction SilentlyContinue)
        throw ('UE '+$Stage+'未输出本阶段成功标记，拒绝标记为通过：'+($tail -join [Environment]::NewLine))
    }
    if($Stage -eq 'GeneratePreview'){
        Write-Output 'GOLD_SPAWNER_GENERATION_PREVIEW_VALIDATED_IN_UE'
        Write-Output '仅在独立Editor生成并清理三份预览；不保存地图，不代表导航、服务器权威、G01～G16全项或Cook验收。'
    }else{
        Write-Output 'SPATIAL_MASK_SNAPSHOT_VALIDATED_IN_UE'
        Write-Output '空间数值探针未执行真实网格生成、导航、服务器权威、G01～G16完整矩阵或Cook。'
    }
}
catch{
    Write-Error $_.Exception.Message
    exit 1
}
finally{
    Write-Output ('GoldLevel '+$Stage+'证据目录：'+$directory)
}
