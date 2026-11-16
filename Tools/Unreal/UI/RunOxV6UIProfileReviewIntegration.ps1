#requires -Version 7
<#
SSLM丑牛V6能力栏纯UI展示数据一键接线：
核对真实唯一牛UIProfile→备份现有6占位→写8条纯表现映射→独立UE5.8冷回读→64/48像素技术门禁。
必须在正式用户要求下显式执行；禁改GAS授权、V6人工审批标志、其它生肖包。
不要使用Content Browser之外的假文件冒充原生.uasset。
#>
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$editor='D:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project=Join-Path $root 'Game/DivineBeastsArena.uproject'
$author=Join-Path $root 'Tools/Unreal/UI/BindOxV6ReviewUIProfileUE58.py'
$verifier=Join-Path $root 'Tools/Unreal/UI/VerifyOxV6ReviewUIProfileUE58.py'
$logRoot=Join-Path $root 'Game/Saved/Validation/OxV6ArtProfileIntegration'
New-Item -Force -ItemType Directory -Path $logRoot|Out-Null
foreach($p in @($editor,$project,$author,$verifier)){
    if(!(Test-Path -LiteralPath $p -PathType Leaf)){throw "关键资产制作入口不存在："+$p}
}
$env:SSLM_OX_V6_PROFILE_MODE='apply'
$writeLog=Join-Path $logRoot 'UIProfileAuthor.log'
$writeStd=Join-Path $logRoot 'UIProfileAuthor_stdout.txt'
& $editor $project '-run=pythonscript' ("-script="+$author) '-SSLMOxV6ReviewOnlyProfileAuthoring' '-unattended' '-nop4' '-nosplash' '-nullrhi' '-nosound' '-stdout' '-FullStdOutLogOutput' ("-abslog="+$writeLog) *> $writeStd
$authorExit=$LASTEXITCODE
Write-Output ("SSLM_OX_UI_AUTHOR_EXIT "+$authorExit)
if(Test-Path $writeLog){Select-String -LiteralPath $writeLog -Pattern 'SSLM_OX_V6_PROFILE_|LogPython: Error:'|Select-Object -Last 24|ForEach-Object{$_.Line}}
if($authorExit -ne 0){throw '正式UE5.8 Ox UIProfile写入失败'}
if(!(Select-String -LiteralPath $writeLog -SimpleMatch 'SSLM_OX_V6_PROFILE_REVIEW_ONLY_BOUND' -Quiet) -and
   !(Select-String -LiteralPath $writeLog -SimpleMatch 'SSLM_OX_V6_PROFILE_ALREADY_BOUND 8' -Quiet)){
   throw '原生UIProfile未确认成功增补8项'
}

Remove-Item Env:SSLM_OX_V6_PROFILE_MODE -ErrorAction SilentlyContinue
$verifyLog=Join-Path $logRoot 'UIProfileColdReload.log'
$verifyStd=Join-Path $logRoot 'UIProfileColdReload_stdout.txt'
& $editor $project '-run=pythonscript' ("-script="+$verifier) '-unattended' '-nop4' '-nosplash' '-nullrhi' '-nosound' '-stdout' '-FullStdOutLogOutput' ("-abslog="+$verifyLog) *> $verifyStd
$verifyExit=$LASTEXITCODE
Write-Output ("SSLM_OX_UI_PROFILE_COLD_EXIT "+$verifyExit)
if(Test-Path $verifyLog){Select-String -LiteralPath $verifyLog -Pattern 'SSLM_OX_V6_PROFILE_ICON_COLD_OK|SSLM_OX_V6_PROFILE_COLD_VERIFY_OK|LogPython: Error:'|Select-Object -Last 20|ForEach-Object {$_.Line}}
if($verifyExit -ne 0 -or !(Select-String -LiteralPath $verifyLog -SimpleMatch 'SSLM_OX_V6_PROFILE_COLD_VERIFY_OK 8 placeholders 6 profileEntries 14 unapprovedGrants 0' -Quiet)){
    throw '8条Profile映射、旧6个占位及0V6授予冷回读未全部通过'
}
$qaScript=Join-Path $root 'Tools/Unreal/UI/AuditOxV6SkillIconSmallSizes.ps1'
$qaResult=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Ox/SourceArt/UI/SkillIcons/V6/OxV6_SmallSizeTechnicalReview.json'
if(!(Test-Path -LiteralPath $qaResult)){
    & $qaScript
    if($LASTEXITCODE -ne 0){throw '64与48PNG质量门禁未通过'}
}
$data=Get-Content -LiteralPath $qaResult -Raw|ConvertFrom-Json
if($data.technicalSizeGate -ne 'PASS' -or $data.technicalDuplicateByteGate -ne 'PASS' -or
   $data.review64Count -ne 8 -or $data.runtime48Count -ne 8){
   throw '小图标QA记录与交付范围不一致'
}
Write-Output 'SSLM_OX_V6_UI_PROFILE_AND_SMALL_ART_TECHNICAL_DONE 8 6 14'
Write-Output 'SSLM_OX_V6_RELEASE_PENDING_ART_AND_GAS_AUTHORIZATION'
