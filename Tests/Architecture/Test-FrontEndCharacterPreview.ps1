param()

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Read-Text([string]$RelativePath) {
    return [System.IO.File]::ReadAllText(
        (Join-Path $Root $RelativePath),
        [System.Text.Encoding]::UTF8)
}

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

$PackRoot = Join-Path $Root "Game\Plugins\DivineBeasts\ContentPacks\Presentation\DBAFrontEndPack"
$FrontMap = Join-Path $PackRoot "Content\Maps\L_DBA_FrontEnd.umap"
$StudioMap = Join-Path $PackRoot "Content\Maps\L_DBA_CharacterStudio.umap"

$FrontMapExists = Test-Path $FrontMap
$StudioMapExists = Test-Path $StudioMap
Assert-True $FrontMapExists "缺少L_DBA_FrontEnd真实.umap。"
Assert-True $StudioMapExists "缺少L_DBA_CharacterStudio真实.umap。"

$DescriptorText = Read-Text "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack/DBAFrontEndPack.uplugin"
$Descriptor = $DescriptorText | ConvertFrom-Json
Assert-True ([bool]$Descriptor.CanContainContent) "DBAFrontEndPack必须允许内容。"
Assert-True ($null -eq $Descriptor.Modules) "DBAFrontEndPack必须保持纯内容插件。"

$PresentationDependency = @(
    $Descriptor.Plugins |
        Where-Object { $_.Name -eq "GamePlatformPresentation" }
)
Assert-True ($PresentationDependency.Count -eq 1) "DBAFrontEndPack必须且只能声明一个GamePlatformPresentation依赖项。"
$AllowedTargets = @($PresentationDependency[0].TargetAllowList)
Assert-True ($AllowedTargets.Count -eq 2) "DBAFrontEndPack必须只允许Client/Editor两个目标。"
Assert-True ($AllowedTargets.Contains("Client")) "DBAFrontEndPack缺少Client目标。"
Assert-True ($AllowedTargets.Contains("Editor")) "DBAFrontEndPack缺少Editor目标。"

$ClientTarget = Read-Text "Game/Source/DivineBeastsArenaClient.Target.cs"
$EditorTarget = Read-Text "Game/Source/DivineBeastsArenaEditor.Target.cs"
$ServerTarget = Read-Text "Game/Source/DivineBeastsArenaServer.Target.cs"
Assert-True ($ClientTarget.Contains('EnablePlugins.Add("DBAFrontEndPack")')) "Client Target未启用DBAFrontEndPack。"
Assert-True ($EditorTarget.Contains('EnablePlugins.Add("DBAFrontEndPack")')) "Editor Target未启用DBAFrontEndPack。"
Assert-True (-not $ServerTarget.Contains("DBAFrontEndPack")) "Server Target禁止启用DBAFrontEndPack。"

$StageHeader = Read-Text "Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Public/Preview/GamePlatformCharacterPreviewStage.h"
$StageCpp = Read-Text "Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/Preview/GamePlatformCharacterPreviewStage.cpp"
$StageText = $StageHeader + $StageCpp
Assert-True (-not $StageText.Contains("DivineBeasts")) "平台Preview Stage不得出现DivineBeasts项目类型。"
Assert-True (-not $StageText.Contains("Zodiac")) "平台Preview Stage不得出现生肖身份。"
Assert-True (-not $StageText.Contains("Hero.Zodiac")) "平台Preview Stage不得出现项目Hero ID。"
Assert-True ($StageCpp.Contains("PrimaryActorTick.bCanEverTick = false")) "Preview Stage必须无Tick。"
Assert-True ($StageCpp.Contains("bReplicates = false")) "Preview Stage必须禁止复制。"

$PreviewCpp = Read-Text "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Characters/DivineBeastsCharacterPreviewSubsystem.cpp"
Assert-True ($PreviewCpp.Contains("/DBAFrontEndPack/Maps/L_DBA_CharacterStudio")) "项目Preview Subsystem必须按需流送CharacterStudio。"
Assert-True ($PreviewCpp.Contains("RequestDefaultProfile")) "项目Preview必须复用既有Appearance Profile。"
Assert-True (-not $PreviewCpp.Contains("SpawnActor")) "Preview Subsystem不得直接SpawnActor。"
Assert-True (-not $PreviewCpp.Contains("->Possess(")) "Preview Subsystem不得直接Possess。"

$UIBuild = Read-Text "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/DivineBeastsUIClient.Build.cs"
Assert-True ($UIBuild.Contains('"DivineBeastsPresentationClient"')) "UI模块必须私有依赖项目表现客户端以驱动预览。"

$WorldFiles = Get-ChildItem (Join-Path $Root "Game\Plugins\DivineBeasts\DBAWorlds") -Recurse -File -Include *.h,*.cpp,*.uplugin,*.md
foreach ($File in $WorldFiles) {
    $Text = [System.IO.File]::ReadAllText(
        $File.FullName,
        [System.Text.Encoding]::UTF8)
    $HasFrontEndReference =
        $Text.Contains("DBAFrontEndPack") -or
        $Text.Contains("L_DBA_FrontEnd") -or
        $Text.Contains("L_DBA_CharacterStudio")
    Assert-True (-not $HasFrontEndReference) "DBAWorlds不得拥有前端预览地图: $($File.FullName)"
}

Write-Output "FrontEndCharacterPreviewArchitecture=PASS"
Write-Output "Maps=2"
Write-Output "ClientEditorOnly=PASS"
Write-Output "NoServerWorldDependency=PASS"
