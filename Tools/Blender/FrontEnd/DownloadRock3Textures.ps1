#requires -Version 7.0
<#
.SYNOPSIS
下载经人工核对许可的 Poly Haven Rock 3 原始 PBR 文件，保留其真实字节。
.DESCRIPTION
仅写第三层 DBAFrontEndPack 的 SourceArt/ThirdParty；不编辑第三方图像，不生成UE资产。
来源与CC0说明见同目录许可文档。显式URL与MD5锁定；下载失败/哈希不符立即退出。
既有正确文件复用，错误文件不覆盖；新文件先写临时路径，校验后发布。
#>
param()
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$destination=Join-Path $workspace 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack/SourceArt/Blender/ZodiacSanctuary/ThirdParty/PolyHaven/Rock3'
$null=New-Item -ItemType Directory -Path $destination -Force
$files=@(
    @{Name='T_DBA_Sanctuary_Rock.png'; Source='rock_3_diff_2k.png'; MD5='6ce05e52225a076d7ec67a54ce07f6ab'},
    @{Name='T_DBA_Sanctuary_Rock_Normal.png'; Source='rock_3_nor_gl_2k.png'; MD5='9c894aae6ffa61e94209b8551f87e5d8'},
    @{Name='T_DBA_Sanctuary_Rock_ORM.png'; Source='rock_3_arm_2k.png'; MD5='8bae2ecb2ea2c1a6eba778628f631e9c'}
)
foreach ($entry in $files) {
    $target=Join-Path $destination $entry.Name
    if (Test-Path -LiteralPath $target) {
        if ((Get-FileHash -LiteralPath $target -Algorithm MD5).Hash.ToLowerInvariant() -ne $entry.MD5) { throw "既有第三方源图不符，拒绝覆盖：$target" }
    } else {
        $pending=$target+'.download'
        Invoke-WebRequest -Uri ('https://dl.polyhaven.org/file/ph-assets/Textures/png/2k/rock_3/'+$entry.Source) -OutFile $pending -Headers @{'User-Agent'='DivineBeastsWorkspace-ArtImporter/1.0'}
        if ((Get-FileHash -LiteralPath $pending -Algorithm MD5).Hash.ToLowerInvariant() -ne $entry.MD5) { throw "原始源图下载哈希不符：$pending" }
        Move-Item -LiteralPath $pending -Destination $target
    }
    Get-FileHash -LiteralPath $target -Algorithm SHA256 | Select-Object Path,Hash
}
