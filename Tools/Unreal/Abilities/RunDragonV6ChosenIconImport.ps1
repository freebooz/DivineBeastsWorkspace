#requires -Version 7
<#
.SYNOPSIS
  神兽联盟辰龙V6八张真实Texture2D图标：独立UE5.8导入及冷重载。
.DESCRIPTION
  默认只读；-Apply只在16张已选AI源图及48缩图到位后开始原生导入。
  不覆盖5张V1、不修改UIProfile或AbilitySet。-VerifyOnly在第二个UE进程回读。
#>
[CmdletBinding()]
param([switch]$Apply,[switch]$VerifyOnly,[string]$EngineRoot='D:/UnrealEngine-5.8.0-release')
$ErrorActionPreference='Stop'
if($Apply -and $VerifyOnly){throw '导入与验证不能同时触发'}
$root=[System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$py=Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$author=Join-Path $root 'Tools/Unreal/Abilities/ImportDragonV6ChosenIconsUE58.py'
$project=Join-Path $root 'Game/DivineBeastsArena.uproject'
foreach($f in @($py,$editor,$author,$project)){if(!(Test-Path -LiteralPath $f)){throw ('必要工程文件不存在：'+$f)}}
Push-Location $root
try {
 & $py -m py_compile $author
 if($LASTEXITCODE -ne 0){throw '图标导入脚本语法错误'}
 $env:SSLM_DRAGON_V6_ICON_MODE='inspect'
 $preflight=(& $py $author | Out-String)
 if($LASTEXITCODE -ne 0){throw '已选高清图来源检查异常'}
 Write-Output $preflight
 if(!$Apply -and !$VerifyOnly){Write-Output 'DRAGON_V6_ICON_INSPECT_ONLY_NO_NATIVE_IMPORT';return}
 if($Apply -and $preflight -notmatch 'SSLM_DRAGON_V6_TEXTURE_IMPORT_READY'){
  throw '8份AI图源及8份48像素PNG尚未复制到辰龙内容包；禁止用概念草稿替代'
 }
 $others=@(Get-CimInstance Win32_Process|Where-Object{$_.Name -eq 'UnrealEditor.exe' -and $_.CommandLine -match 'DivineBeastsArena\.uproject'})
 if($others.Count -gt 0){throw '其他同工程UE编辑器正在使用资源，请先安全关闭'}
 $folder=Join-Path $root 'Game/Saved/Validation/DragonV6SelectedImages_20261018'
 if(!(Test-Path -LiteralPath $folder)){New-Item -ItemType Directory -Path $folder -Force|Out-Null}
 $filename=if($Apply){'EightIconsNativeImportUE58.log'}else{'EightIconsNativeColdReloadUE58.log'}
 $log=Join-Path $folder $filename
 if(Test-Path -LiteralPath $log){throw '本轮原生日志存在，不重复覆盖历史证据'}
 $env:SSLM_DRAGON_V6_ICON_MODE=if($Apply){'apply'}else{'verify'}
 $args=@($project,('-ExecutePythonScript='+$author),'-unattended','-NoSplash','-NoCompile','-NoLiveCoding','-NoHotReload','-DisablePlugins=Monolith','-NullRHI',('-abslog='+$log))
 & $editor @args
 if(!(Test-Path -LiteralPath $log)){throw 'UE没有写入导入或冷重载真实日志'}
 $marker=if($Apply){'SSLM_DRAGON_V6_NATIVE_UE_EIGHT_TEXTURES_SAVED'}else{'SSLM_DRAGON_V6_EIGHT_TEXTURES_IN_HERO_PACK_COLD_VERIFY_PASS'}
 $pass=@(Select-String -LiteralPath $log -SimpleMatch -Pattern $marker)
 $bad=@(Select-String -LiteralPath $log -SimpleMatch -Pattern 'LogPython: Error:','LogEditorPythonExecuter: Error:','LogSavePackage: Error:')
 if($pass.Count -ne 1 -or $bad.Count -ne 0){
  $bad|Select-Object -Last 10|ForEach-Object{Write-Output $_.Line}
  throw 'UE真实图标导入或冷重载未通过；编辑器进程返回0不能算成功'
 }
 $receipt=Join-Path $root 'Saved/ArtReview/DragonV6Selected8NativeIcons_20261018/UEImportedTextures.json'
 if(!(Test-Path -LiteralPath $receipt)){throw '缺少8张真实Texture2D导入清单'}
 $actual=Get-Content -LiteralPath $receipt -Raw -Encoding UTF8|ConvertFrom-Json
 if($actual.importedCount -ne 8 -or $actual.items.Count -ne 8 -or $actual.profileBound -ne $false){
  throw '原生回执数量不对或正式UIProfile被非法变更'
 }
 if($VerifyOnly -and !$actual.coldVerified){throw 'UE独立冷重载尚未确认'}
 Write-Output ('SSLM_DRAGON_V6_IMPORT_RESULT success=true coldVerify='+$actual.coldVerified+' ProfileNotModified=true')
}finally{
 Remove-Item Env:SSLM_DRAGON_V6_ICON_MODE -ErrorAction SilentlyContinue
 Pop-Location
}
