#requires -Version 7.0
<#
.SYNOPSIS
SSLM（神兽联盟）跨项目定义编号到主资产身份公共基类复用静态检查。
.DESCRIPTION
仅读取源代码验证真实平台基类与各业务消费者依赖，不调用UE工具、
不运行C++测试、不编译或保存任何 .uasset/.umap。检查通过不代表运行通过。
#>
param([string]$WorkspaceRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)
$ErrorActionPreference='Stop'
$root=(Resolve-Path $WorkspaceRoot).Path
$issues=[Collections.Generic.List[string]]::new()
$checked=0

function Check([string]$RelativePath,[string]$Pattern,[string]$Label){
    $script:checked++
    $path=Join-Path $root $RelativePath
    if(-not(Test-Path -LiteralPath $path)){
        $script:issues.Add("缺少$Label 文件：$RelativePath")
        return
    }
    if((Get-Content $path -Raw -Encoding UTF8) -notmatch $Pattern){
        $script:issues.Add("$Label 合同未接通：$RelativePath")
    }
}
$platform='Game/Plugins/GamePlatform'
$data="$platform/Foundation/GamePlatformData/Source/GamePlatformData"
$sfx="$platform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient"
$vfx="$platform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient"
$project='Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient'

Check "$data/Public/Definitions/GamePlatformPrimaryDataAsset.h" 'static EGamePlatformDefinitionIdBuildStatus BuildDefinitionAssetId\(' '唯一平台公共转换声明'
Check "$data/Private/Definitions/GamePlatformDefinitionBase.cpp" 'UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId\(' '唯一平台公共转换实现'
Check "$sfx/Private/Policy/GamePlatformSFXPolicy.cpp" 'UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId\(' '声音消费者'
Check "$vfx/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp" 'UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId\(' '特效消费者'
Check "$project/Private/DivineBeastsPresentationClientSubsystem.cpp" 'UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId\(' '项目表现逻辑预载消费者'
Check "$data/Private/Tests/GamePlatformDefinitionAssetIdResolutionTests.cpp" 'GamePlatform.Data.Definitions.SharedAssetIdResolution' '公共转换原生测试源码'
Check "$sfx/Private/Policy/GamePlatformSFXPolicy.cpp" 'SFX.DefinitionIdInvalid' '原有声音业务错误码'
Check "$vfx/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp" 'bool BuildDefinitionAssetId\(' '原特效调用方布尔包装'
Check "$data/Public/Definitions/GamePlatformPrimaryDataAsset.h" 'FPrimaryAssetType DefinitionAssetType\(' '既有稳定主资产类型'

$checked++
$dataHeader=Get-Content (Join-Path $root "$data/Public/Definitions/GamePlatformPrimaryDataAsset.h") -Raw
if($dataHeader -match '(?i)DBAContentPack|DivineBeasts|MobaCommon'){
    $issues.Add('平台数据Public类型反向引用了游戏项目或MOBA类型')
}
$checked++
$body=Get-Content (Join-Path $root "$data/Private/Definitions/GamePlatformDefinitionBase.cpp") -Raw
if($body -notmatch 'OutAssetId\s*=\s*FPrimaryAssetId\(\);'){
    $issues.Add('平台公共身份转换应在失败前清空输出')
}

if($issues.Count -gt 0){
    $issues|ForEach-Object{Write-Warning $_}
    Write-Output ("跨模块定义身份复用静态检查失败："+$issues.Count+'项，检查总数='+$checked)
    exit 1
}
Write-Output ("跨模块定义身份复用静态检查通过："+$checked+'项；未编译/未运行UE测试。')
