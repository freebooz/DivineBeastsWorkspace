#requires -Version 5.1
<#
.SYNOPSIS
UE5.8 GoldLevel（PCG金标准关卡）已保存几何/图空间快照只读探针。
.DESCRIPTION
禁止与其它UE编辑器/UBT构建并发；先核对PCGEditor DLL编译时间晚于本阶段C++变更。
只运行独立Editor命令行并检查指定成功日志，不重新生成网格、不保存地图、不做Cook。
#>
[CmdletBinding()]
param([string]$EngineRoot = $env:UE_ROOT)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$game=Join-Path $root 'Game/DivineBeastsArena.uproject'
$map=Join-Path $root 'Game/Content/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1.umap'
$module=Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Binaries/Win64/UnrealEditor-GamePlatformPCGEditor.dll'
$source=Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldMapAuthoring.cpp'
$commandlet=Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldAssetsCommandlet.cpp'
$directory=Join-Path $root ('Saved/Validation/GamePlatformPCG/SpatialProbes/'+[guid]::NewGuid().ToString())
New-Item -Path $directory -ItemType Directory -Force |Out-Null
$stdout=Join-Path $directory 'UE_Editor_SpatialProbe.stdout.log'
$stderr=Join-Path $directory 'UE_Editor_SpatialProbe.stderr.log'
try {
    if(!$EngineRoot){throw '必须指定UE_ROOT（UE5.8引擎根目录）或-EngineRoot。'}
    $engine=(Resolve-Path -LiteralPath $EngineRoot).Path
    $version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8|ConvertFrom-Json
    if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw '仅支持已锁定的UE5.8。'}
    $editor=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    foreach($file in @($game,$map,$module,$source,$commandlet,$editor)){
        if(!(Test-Path -LiteralPath $file)){throw ('PCG真实地图或模块缺失：'+$file)}
    }
    $compiled=(Get-Item $module).LastWriteTimeUtc
    if((Get-Item $source).LastWriteTimeUtc -gt $compiled -or
       (Get-Item $commandlet).LastWriteTimeUtc -gt $compiled){
        throw 'GamePlatformPCGEditor DLL早于SpatialProbe源码；必须先完成正式UE模块编译和链接。'
    }
    if(@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count){
        throw '另一个UE编辑器正在运行，禁止并行载入同一项目地图。'
    }
    $ubt=@(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" |
        Where-Object { $_.CommandLine -match 'UnrealBuildTool' })
    if($ubt.Count){throw 'UE构建正在使用共享中间文件，拒绝争用。'}
    $disable=@('Monolith','DBAClient','DBAArena','GamePlatformSurface',
        'DBAUIPack_Core','DBAFrontEndPack','DBAContentPack_Common',
        'DBAHeroPack_Rat','DBAHeroPack_Ox','DBAHeroPack_Tiger','DBAHeroPack_Rabbit',
        'DBAHeroPack_Dragon','DBAHeroPack_Snake','DBAHeroPack_Horse','DBAHeroPack_Goat',
        'DBAHeroPack_Monkey','DBAHeroPack_Rooster','DBAHeroPack_Dog','DBAHeroPack_Boar')
    $arguments=@($game,'-run=GamePlatformPCGGoldAssets','-Stage=SpatialProbe',
        "-DisablePlugins=$($disable -join ',')",
        '-EnablePlugins=CommonUI,DBAGameplay,DBAWorlds,DBAWorldPack_Village',
        '-NoCompile','-unattended','-nop4','-nosplash','-nosound','-nullrhi','-stdout')
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -PassThru -Wait -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    Write-Output ('SPATIAL_PROBE_UE_EXIT='+$process.ExitCode)
    $marker='GoldLevel只读空间探针已核验'
    $matched=(Test-Path $stdout) -and [bool](Select-String -LiteralPath $stdout -Pattern $marker -SimpleMatch -Quiet)
    if($process.ExitCode -ne 0 -or !$matched){
        $tail=@(Get-Content $stdout -Encoding UTF8 -Tail 22 -ErrorAction SilentlyContinue)
        throw ('UE空间探针无成功标记，拒绝标记为通过：'+($tail -join [Environment]::NewLine))
    }
    Write-Output 'SPATIAL_MASK_SNAPSHOT_VALIDATED_IN_UE'
    Write-Output '注意：没有执行真实网格生成、导航、服务器权威、G01～G16完整矩阵或Cook。'
}
catch{
    Write-Error $_.Exception.Message
    exit 1
}
finally{
    Write-Output ('SpatialProbe证据目录：'+$directory)
}
