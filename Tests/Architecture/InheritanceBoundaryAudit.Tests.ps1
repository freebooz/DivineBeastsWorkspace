Describe '三层类型继承与Public API边界' {
    $modulePath = Join-Path $PSScriptRoot 'InheritanceBoundaryAudit.psm1'
    Import-Module $modulePath -Force

    function New-TestHeader {
        param(
            [Parameter(Mandatory)][string]$Root,
            [Parameter(Mandatory)][string]$Layer,
            [Parameter(Mandatory)][string]$Plugin,
            [Parameter(Mandatory)][string]$Module,
            [Parameter(Mandatory)][string]$RelativePath,
            [Parameter(Mandatory)][string]$Content
        )
        $path = Join-Path $Root "Game/Plugins/$Layer/$Plugin/Source/$Module/$RelativePath"
        $null = New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force
        Set-Content -LiteralPath $path -Value $Content -Encoding UTF8
        return $path
    }

    It '接受平台到MOBA到项目以及项目直继承平台的单向继承' {
        $root = Join-Path $TestDrive 'Valid'
        New-TestHeader $root 'GamePlatform' 'Foundation/Base' 'Base' 'Public/PlatformBase.h' @'
class PLATFORM_API UPlatformBase { };
'@ | Out-Null
        New-TestHeader $root 'MobaCommon' 'Arena' 'Arena' 'Public/MobaBase.h' @'
class MOBA_API UMobaBase : public UPlatformBase { };
'@ | Out-Null
        New-TestHeader $root 'DivineBeasts' 'Gameplay' 'Gameplay' 'Public/ProjectTypes.h' @'
class DBA_API UProjectArena : public UMobaBase { };
class DBA_API UProjectDirect : public UPlatformBase { };
'@ | Out-Null

        $report = Test-InheritanceBoundaries -WorkspaceRoot $root
        ($report.Findings -join [Environment]::NewLine) | Should Be ''
        $report.Passed | Should Be $true
    }

    It '拒绝GamePlatform反向继承DivineBeasts类型' {
        $root = Join-Path $TestDrive 'Reverse'
        New-TestHeader $root 'DivineBeasts' 'Gameplay' 'Gameplay' 'Public/ProjectBase.h' @'
class DBA_API UProjectBase { };
'@ | Out-Null
        New-TestHeader $root 'GamePlatform' 'Gameplay/Bad' 'Bad' 'Public/BadPlatform.h' @'
class PLATFORM_API UBadPlatform : public UProjectBase { };
'@ | Out-Null

        $report = Test-InheritanceBoundaries -WorkspaceRoot $root
        $report.Passed | Should Be $false
        ($report.Findings -join [Environment]::NewLine) | Should Match '反向继承'
    }

    It '拒绝MobaCommon Public API引用DivineBeasts公开类型' {
        $root = Join-Path $TestDrive 'Pollution'
        New-TestHeader $root 'DivineBeasts' 'Gameplay' 'Gameplay' 'Public/ProjectType.h' @'
class DBA_API UProjectType { };
'@ | Out-Null
        New-TestHeader $root 'MobaCommon' 'Arena' 'Arena' 'Public/MobaApi.h' @'
class MOBA_API UMobaApi
{
public:
    UProjectType* GetProjectType() const;
};
'@ | Out-Null

        $report = Test-InheritanceBoundaries -WorkspaceRoot $root
        $report.Passed | Should Be $false
        ($report.Findings -join [Environment]::NewLine) | Should Match '低层Public API引用上层类型'
    }

    It '拒绝跨层继承Private类型' {
        $root = Join-Path $TestDrive 'PrivateBase'
        New-TestHeader $root 'GamePlatform' 'Gameplay/Hidden' 'Hidden' 'Private/HiddenBase.h' @'
class PLATFORM_API UHiddenBase { };
'@ | Out-Null
        New-TestHeader $root 'DivineBeasts' 'Gameplay' 'Gameplay' 'Public/ProjectType.h' @'
class DBA_API UProjectType : public UHiddenBase { };
'@ | Out-Null

        $report = Test-InheritanceBoundaries -WorkspaceRoot $root
        $report.Passed | Should Be $false
        ($report.Findings -join [Environment]::NewLine) | Should Match 'Private'
    }

    It '注释中的上层类型名称不会制造误报' {
        $root = Join-Path $TestDrive 'Comments'
        New-TestHeader $root 'DivineBeasts' 'Gameplay' 'Gameplay' 'Public/ProjectType.h' @'
class DBA_API UProjectType { };
'@ | Out-Null
        New-TestHeader $root 'GamePlatform' 'Foundation/Base' 'Base' 'Public/PlatformBase.h' @'
/** 示例文字提到 UProjectType，但不是代码依赖。 */
class PLATFORM_API UPlatformBase { };
'@ | Out-Null

        $report = Test-InheritanceBoundaries -WorkspaceRoot $root
        ($report.Findings -join [Environment]::NewLine) | Should Be ''
        $report.Passed | Should Be $true
    }
}
