#requires -Version 5.1
<#
.SYNOPSIS
《神兽联盟》PBR图集ZIP从Windows下载目录安全移交至DivineBeastsWorkspace工作区。
.DESCRIPTION
当前ChatGPT上传文件与远程HWebCodex Runner未建立可信文件转发。此脚本仅读取本机下载目录，
匹配精确ZIP SHA-256后原子复制到项目Incoming；不触碰.uasset、现有材质和他人工作。
不允许公共网盘/自动上传/非用户批准的ZIP版本。禁止静默覆盖已存在的项目源素材。
.PARAMETER SourcePath
用户通过浏览器下载的Windows ZIP路径，可省略让脚本搜索标准Downloads/Desktop等目录。
#>
[CmdletBinding()]
param([string]$SourcePath)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$destinationRoot=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature/Incoming'
$canonicalName='DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip'
$alternateName='DivineBeasts_NiagaraPBR_AtlasAssets_20261011(1).zip'
$expectedSha256='07c5b03e056a0318af8c2368cc3a06c39ff2ef26c181b2912ef2f78f57084015'
if(!(Test-Path $destinationRoot)){New-Item -ItemType Directory -Path $destinationRoot -Force|Out-Null}
$target=Join-Path $destinationRoot $canonicalName
if(Test-Path $target -PathType Leaf){
    $sha=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
    if($sha -ne $expectedSha256){throw '项目Incoming已有不同版本的同名源ZIP，拒绝覆盖：'+$target}
    Write-Output ('PBR_SOURCE_ALREADY_STAGED '+$target)
    exit 0
}
$found=$null
if($SourcePath){
    if(!(Test-Path -LiteralPath $SourcePath -PathType Leaf)){throw '指定的Windows ZIP不存在：'+$SourcePath}
    $found=(Resolve-Path -LiteralPath $SourcePath).Path
}else{
    $folders=@(
        (Join-Path $env:USERPROFILE 'Downloads'),
        (Join-Path $env:USERPROFILE 'Desktop'),
        (Join-Path $env:USERPROFILE 'OneDrive/Downloads'),
        (Join-Path $env:USERPROFILE 'OneDrive/Desktop'),
        'C:/Users/Public/Downloads',
        'D:/Downloads',
        'E:/Downloads'
    )
    foreach($folder in $folders){
        if(!(Test-Path -LiteralPath $folder -PathType Container)){continue}
        foreach($name in @($canonicalName,$alternateName)){
            $candidate=Join-Path $folder $name
            if(Test-Path -LiteralPath $candidate -PathType Leaf){
                $sha=(Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash.ToLowerInvariant()
                if($sha -eq $expectedSha256){
                    $found=(Resolve-Path -LiteralPath $candidate).Path
                    break
                }
                Write-Output ('PBR_SOURCE_HASH_MISMATCH_SKIPPED '+$candidate)
            }
        }
        if($found){break}
    }
}
if(!$found){
    $downloads=Join-Path $env:USERPROFILE 'Downloads'
    if(Test-Path $downloads){Start-Process explorer.exe -ArgumentList @($downloads)}
    Write-Output ('PBR_SOURCE_WAITING_FOR_WINDOWS_DOWNLOAD expected='+$canonicalName)
    Write-Output ('PBR_SOURCE_TARGET='+$target)
    Write-Output '用户应从ChatGPT的sandbox下载链接下载ZIP到本机Downloads；再次运行脚本将自动校验并安全复制。'
    exit 2
}
$sourceHash=(Get-FileHash -LiteralPath $found -Algorithm SHA256).Hash.ToLowerInvariant()
if($sourceHash -ne $expectedSha256){throw '用户提供的ZIP哈希与本对话审核版本不符：'+$found}
if((Get-Item -LiteralPath $found).Length -ne 28051967){throw '用户提供的ZIP字节数错误，禁止导入：'+$found}
$free=(Get-PSDrive -Name 'E' -ErrorAction SilentlyContinue).Free
if($null -ne $free -and $free -lt 800MB){throw 'E盘剩余空间不足800MB，禁止继续暂存ZIP'}
$temporary=Join-Path $destinationRoot ($canonicalName+'.staging')
if(Test-Path $temporary){throw '已存在未处理的ZIP临时导入文件，拒绝覆盖：'+$temporary}
try{
    Copy-Item -LiteralPath $found -Destination $temporary -ErrorAction Stop
    $stagingHash=(Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash.ToLowerInvariant()
    if($stagingHash -ne $expectedSha256){throw 'ZIP复制后SHA-256异常，拒绝进入引擎'}
    [IO.File]::Move($temporary,$target)
    Write-Output ('PBR_SOURCE_STAGED '+$target)
    Write-Output ('PBR_SOURCE_SHA256 '+$stagingHash)
}finally{
    if(Test-Path $temporary){Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue}
}
