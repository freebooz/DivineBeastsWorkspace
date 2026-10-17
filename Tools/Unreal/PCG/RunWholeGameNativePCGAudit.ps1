#requires -Version 5.1
<#
.SYNOPSIS
SSLM三世界PCG原生地图审计入口（当前可用Village、MainArena）。
.DESCRIPTION
两张正式地图分别由独立UE5.8编辑器进程加载，审计原生Actor、Landscape与PCGComponent。
只读；不运行Generate/地图保存/Server权威。检查本次真实日志成功标记、Python异常及地图原文件SHA。
其他UE Editor或UnrealBuildTool在运行时拒绝执行，绝不停止其他任务。
.PARAMETER EngineRoot
锁定UE5.8安装根路径。
.PARAMETER Role
Village/MainArena或All；OpenWorld地图尚未登记，不能伪造审计。
#>
[CmdletBinding()]
param(
    [string]$EngineRoot=$env:UE_ROOT,
    [ValidateSet('Village','MainArena','All')][string]$Role='All'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$project=Join-Path $root 'Game/DivineBeastsArena.uproject'
$script=Join-Path $PSScriptRoot 'AuditWorldPCGLevelUE58.py'
$evidence=Join-Path $root ('Saved/Validation/GamePlatformPCG/NativeWorldAudit/'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence -Force|Out-Null
$records=[Collections.Generic.List[object]]::new()
try {
    if([string]::IsNullOrWhiteSpace($EngineRoot)){throw '必须指定锁定UE5.8引擎'}
    $engine=(Resolve-Path -LiteralPath $EngineRoot).Path
    $version=Get-Content -LiteralPath (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8|ConvertFrom-Json
    if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw '只支持UE5.8'}
    $editor=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    foreach($file in @($editor,$project,$script)){if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw ('缺少必要工程/工具：'+$file)}}
    if(@(Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool -ErrorAction SilentlyContinue).Count){
        throw '其他UE Editor/UBT在运行；拒绝并发加载地图'
    }
    $runningUBT=@(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" |
        Where-Object { $_.CommandLine -match 'UnrealBuildTool' })
    if($runningUBT.Count){throw '已有UE UBT使用共享引擎；禁止争用'}
    $maps=@(
        @{Role='Village';Package='/DBAWorldPack_Village/Maps/L_Village_Start';File='Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'},
        @{Role='MainArena';Package='/DBAWorldPack_MainArena/Maps/L_DBA_MainArena';File='Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_MainArena/Content/Maps/L_DBA_MainArena.umap'}
    )
    foreach($world in $maps){
        if($Role -ne 'All' -and $world.Role -ne $Role){continue}
        $target=Join-Path $root $world.File
        if(!(Test-Path -LiteralPath $target -PathType Leaf)){throw ('缺少正式UE地图：'+$world.File)}
        $before=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        $stdout=Join-Path $evidence ($world.Role+'.console.log')
        $stderr=Join-Path $evidence ($world.Role+'.stderr.log')
        $log=Join-Path $evidence ($world.Role+'.unreal.log')
        $env:SSLM_PCG_AUDIT_MAP=$world.Package
        try {
            $args=@($project,'/Engine/Maps/Entry',("-ExecutePythonScript="+$script),
                '-EnablePlugins=PythonScriptPlugin,CommonUI,DBAGameplay,DBAWorlds,DBAWorldPack_Village',
                '-DisablePlugins=Monolith','-unattended','-nop4','-nosplash','-nullrhi',
                '-NoCompile','-NoCompileEditor','-NoLiveCoding',('-abslog='+$log))
            $process=Start-Process -FilePath $editor -ArgumentList $args -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
            if(!$process.WaitForExit(420000)){
                $process.Kill()
                [void]$process.WaitForExit(5000)
                throw ('本次独立地图审计超过420秒，已停止自有编辑器：'+$world.Role)
            }
            $process.Refresh()
        } finally {
            Remove-Item Env:SSLM_PCG_AUDIT_MAP -ErrorAction SilentlyContinue
        }
        if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $before){
            throw ('正式世界地图在只读审计中发生字节变化：'+$world.File)
        }
        $logBody=if(Test-Path $log){Get-Content -LiteralPath $log -Raw -Encoding UTF8}else{''}
        $marker='SSLM_NATIVE_PCG_WORLD_AUDIT_PASS '+$world.Role
        $ok=$process.ExitCode -eq 0 -and $logBody.Contains($marker) -and
            !$logBody.Contains('LogPython: Error:') -and !$logBody.Contains('Fatal error:')
        $records.Add(@{
            Role=$world.Role;Map=$world.Package;ExitCode=$process.ExitCode;
            MarkerSeen=$logBody.Contains($marker);Passed=$ok;Log=$log;MapSha256=$before
        })
        Write-Output ('SSLM_PCG_NATIVE_AUDIT_RESULT '+$world.Role+' passed='+$ok+' exit='+$process.ExitCode)
        if(!$ok){throw ('独立UE审计失败；必须检查本次原生日志：'+$log)}
    }
    Write-Output 'SSLM_PCG_ALL_REQUESTED_WORLD_AUDITS_COMPLETED_NOT_PRODUCTION_ACCEPTANCE'
    exit 0
}
catch {
    Write-Output ('SSLM_PCG_WORLD_AUDIT_BLOCKED '+$_.Exception.Message)
    exit 1
}
finally {
    $records.ToArray()|ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'result.json') -Encoding UTF8
    Write-Output ('SSLM_PCG_WORLD_AUDIT_EVIDENCE '+$evidence)
}
