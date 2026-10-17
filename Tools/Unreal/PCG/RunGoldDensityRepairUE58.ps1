#requires -Version 5.1
<#
.SYNOPSIS
只针对UE5.8 Gold测试图修复空间排除后缺少零密度过滤器的缺陷。
.DESCRIPTION
源uasset严格限定Gold Realized目录三份；Apply前先备份到Saved并核对SHA，拒绝覆盖同名备份。
每次启动独立UE编辑器，核对Python错误、UE退出码与本次成功标记；Verify再次独立加载。
不修改正式Village/MainArena地图、共享ContentPack资源或服务器碰撞。
.PARAMETER Mode
Inspect只读结构清点；Apply备份后修改仅三份Gold开发图；Verify独立冷加载验证。
#>
[CmdletBinding()]
param(
    [string]$EngineRoot=$env:UE_ROOT,
    [ValidateSet('Inspect','Apply','Verify')][string]$Mode='Inspect'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$project=Join-Path $root 'Game/DivineBeastsArena.uproject'
$script=Join-Path $PSScriptRoot 'RepairGoldDensityFilterUE58.py'
$development=Join-Path $root 'Game/Content/Development/Foundation/PCG/Realized'
$run=Join-Path $root ('Saved/Validation/GamePlatformPCG/GoldDensityRepair/'+[guid]::NewGuid().ToString('N'))
New-Item $run -ItemType Directory -Force | Out-Null
$sourceFiles=@(
    'PCG_Gold_Canopy.uasset','PCG_Gold_Rock.uasset','PCG_Gold_Crop.uasset'
) | ForEach-Object {Join-Path $development $_}
try {
    if(!$EngineRoot){throw '必须指定锁定UE5.8路径'}
    $engine=(Resolve-Path -LiteralPath $EngineRoot).Path
    $version=Get-Content -LiteralPath (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw '引擎必须为UE5.8'}
    $editor=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    foreach($path in @($editor,$project,$script)+$sourceFiles){
        if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw ("当前真实资产/程序缺失："+$path)}
    }
    if(@(Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool -ErrorAction SilentlyContinue).Count){
        throw '当前UE Editor或构建正在运行，拒绝改动已载入的Gold图'
    }
    $before=@{}
    foreach($file in $sourceFiles){$before[$file]=(Get-FileHash $file -Algorithm SHA256).Hash}
    if($Mode -eq 'Apply'){
        $backup=Join-Path $run 'Backup'
        New-Item $backup -ItemType Directory | Out-Null
        foreach($file in $sourceFiles){
            $target=Join-Path $backup ([IO.Path]::GetFileName($file))
            if(Test-Path -LiteralPath $target){throw '备份已存在，拒绝覆盖'}
            Copy-Item -LiteralPath $file -Destination $target
            if((Get-FileHash $target -Algorithm SHA256).Hash -ne $before[$file]){
                throw ('Gold编辑前字节备份校验失败：'+$target)
            }
        }
        Write-Output ('SSLM_GOLD_DENSITY_BACKUP_VERIFIED '+$backup)
    }
    $out=Join-Path $run ($Mode+'.console.log')
    $stderr=Join-Path $run ($Mode+'.stderr.log')
    $native=Join-Path $run ($Mode+'.unreal.log')
    $env:SSLM_GOLD_DENSITY_REPAIR_MODE=$Mode.ToLowerInvariant()
    try{
        $args=@($project,'/Engine/Maps/Entry',("-ExecutePythonScript=$script"),
            '-SSLMGoldDensityRepair','-EnablePlugins=PythonScriptPlugin',
            '-DisablePlugins=Monolith','-NoCompile','-NoCompileEditor',
            '-unattended','-nop4','-nosplash','-nosound','-nullrhi',
            ("-abslog=$native"))
        $p=Start-Process -FilePath $editor -ArgumentList $args -PassThru -RedirectStandardOutput $out -RedirectStandardError $stderr
        if(!$p.WaitForExit(420000)){
            $p.Kill()
            [void]$p.WaitForExit(5000)
            throw '本次Gold修复UE Editor超时；仅终止自己创建的进程'
        }
        $p.Refresh()
    }finally{
        Remove-Item Env:SSLM_GOLD_DENSITY_REPAIR_MODE -ErrorAction SilentlyContinue
    }
    $log=if(Test-Path $native){Get-Content $native -Raw -Encoding UTF8}else{''}
    $marker=switch($Mode){
        'Inspect'{'SSLM_GOLD_DENSITY_INSPECT_PASS 3'}
        'Apply'{'SSLM_GOLD_DENSITY_REPAIR_PASS 3'}
        'Verify'{'SSLM_GOLD_DENSITY_VERIFY_PASS 3'}
    }
    $good=($p.ExitCode -eq 0) -and $log.Contains($marker) -and
        !$log.Contains('LogPython: Error:') -and !$log.Contains('Fatal error:')
    foreach($file in $sourceFiles){
        $after=(Get-FileHash $file -Algorithm SHA256).Hash
        if($Mode -ne 'Apply' -and $after -ne $before[$file]){
            throw ('只读Gold图检查改变了资产源：'+$file)
        }
        if($Mode -eq 'Apply' -and $good -and $after -eq $before[$file]){
            throw ('UE报告已改图，但文件没有实际发生变化：'+$file)
        }
    }
    Write-Output ("SSLM_GOLD_DENSITY_UE_RESULT mode=$Mode exit=$($p.ExitCode) passed=$good")
    if(!$good){throw '独立UE进程未通过本阶段反射校验，请查看：'+$native}
    exit 0
} catch {
    Write-Output ('SSLM_GOLD_DENSITY_REPAIR_BLOCKED '+$_.Exception.Message)
    exit 1
} finally {
    Write-Output ('SSLM_GOLD_DENSITY_EVIDENCE '+$run)
}
