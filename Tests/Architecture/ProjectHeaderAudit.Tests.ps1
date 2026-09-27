# 项目消费者的源码可用性检查，不把描述文件依赖成立误判为接口已实现。
Describe '项目源码引用的自有头文件存在性' {
    $auditModule = Join-Path $PSScriptRoot 'ProjectHeaderAudit.psm1'
    if (Test-Path -LiteralPath $auditModule) { Import-Module $auditModule -Force }

    BeforeEach {
        $root = Join-Path $TestDrive ([guid]::NewGuid().ToString('N'))
        $source = Join-Path $root 'Game/Plugins/DivineBeasts/Fixture/Source/Consumer/Private'
        $public = Join-Path $root 'Game/Plugins/GamePlatform/Fixture/Source/Provider/Public'
        $null = New-Item -ItemType Directory -Path $source,$public -Force
    }

    It '报告缺失接口的文件、行号和include原文' {
        Set-Content -LiteralPath (Join-Path $source 'Consumer.cpp') -Value @('// 调用方', '#include "GamePlatformMissingService.h"')
        $report = Test-ProjectOwnedHeaders -WorkspaceRoot $root
        $report.Passed | Should Be $false
        $report.Findings.Count | Should Be 1
        $report.Findings[0].Include | Should Be 'GamePlatformMissingService.h'
        $report.Findings[0].Line | Should Be 2
        $report.Findings[0].File | Should Match 'Consumer.cpp$'
    }

    It '兼容已有公开头、Shared生成头和UE反射生成头' {
        Set-Content -LiteralPath (Join-Path $public 'GamePlatformService.h') -Value '// 夹具公开服务'
        $generated = Join-Path $root 'Shared/Generated/Cpp'
        $null = New-Item -ItemType Directory -Path $generated -Force
        Set-Content -LiteralPath (Join-Path $generated 'DivineBeastsCatalog.generated.hpp') -Value '// 夹具跨语言生成物'
        Set-Content -LiteralPath (Join-Path $source 'Consumer.cpp') -Value @('#include "GamePlatformService.h"', '#include "DivineBeastsCatalog.generated.hpp"', '#include "DivineBeastsObject.generated.h"', '#include "CoreMinimal.h"')
        $report = Test-ProjectOwnedHeaders -WorkspaceRoot $root
        $report.Passed | Should Be $true
        $report.Findings.Count | Should Be 0
        $report.CheckedIncludes | Should Be 2
    }

    It '忽略注释中旧示例但不忽略真正的条件编译引用' {
        Set-Content -LiteralPath (Join-Path $source 'Consumer.cpp') -Value @('/*', '#include "GamePlatformCommented.h"', '*/', '// #include "GamePlatformCommented.h"', '#if WITH_DEV_AUTOMATION_TESTS', '#include "GamePlatformTestService.h"', '#endif')
        $report = Test-ProjectOwnedHeaders -WorkspaceRoot $root
        $report.Findings.Count | Should Be 1
        $report.Findings[0].Include | Should Be 'GamePlatformTestService.h'
        $report.Findings[0].Line | Should Be 6
    }

    It '没有项目源码时明确失败而不是空扫描通过' {
        $emptyRoot = Join-Path $TestDrive ([guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path $emptyRoot
        { Test-ProjectOwnedHeaders -WorkspaceRoot $emptyRoot } | Should Throw '缺少项目插件源码根'
    }

    It '扫描主工程Source并检查DBA前缀的自有头' {
        Set-Content -LiteralPath (Join-Path $public 'GamePlatformService.h') -Value '// 夹具公开服务'
        Set-Content -LiteralPath (Join-Path $source 'Consumer.cpp') -Value '#include "GamePlatformService.h"'
        $mainSource = Join-Path $root 'Game/Source/DivineBeastsArena'
        $null = New-Item -ItemType Directory -Path $mainSource -Force
        Set-Content -LiteralPath (Join-Path $mainSource 'Main.cpp') -Value '#include "DBAMissingWorldService.h"'
        $report = Test-ProjectOwnedHeaders -WorkspaceRoot $root
        $report.Passed | Should Be $false
        $report.Findings.Count | Should Be 1
        $report.Findings[0].Include | Should Be 'DBAMissingWorldService.h'
    }

    It '主工程已有的自有头也进入存在性索引' {
        $mainSource = Join-Path $root 'Game/Source/DivineBeastsArena'
        $null = New-Item -ItemType Directory -Path $mainSource -Force
        Set-Content -LiteralPath (Join-Path $mainSource 'DivineBeastsMain.h') -Value '// 主工程夹具'
        Set-Content -LiteralPath (Join-Path $source 'Consumer.cpp') -Value '#include "DivineBeastsMain.h"'
        (Test-ProjectOwnedHeaders -WorkspaceRoot $root).Passed | Should Be $true
    }

    It '行注释中的块注释符不能吞掉下一行真正的缺失引用' {
        Set-Content -LiteralPath (Join-Path $source 'Consumer.cpp') -Value @('// /*', '#include "GamePlatformReallyMissing.h"', '// */')
        $report = Test-ProjectOwnedHeaders -WorkspaceRoot $root
        $report.Findings.Count | Should Be 1
        $report.Findings[0].Line | Should Be 2
    }

    It '续行注释和原始字符串中的include不是依赖但保留真实引用行号' {
        Set-Content -LiteralPath (Join-Path $public 'GamePlatformService.h') -Value '// 夹具公开服务'
        Set-Content -LiteralPath (Join-Path $source 'Consumer.cpp') -Value @('// continued \', '#include "GamePlatformInComment.h"', 'const char* Example = R"demo(', '#include "GamePlatformInRawString.h"', ')demo";', '#include "GamePlatformService.h"', '#include "GamePlatformReallyMissing.h"')
        $report = Test-ProjectOwnedHeaders -WorkspaceRoot $root
        $report.CheckedIncludes | Should Be 2
        $report.Findings.Count | Should Be 1
        $report.Findings[0].Include | Should Be 'GamePlatformReallyMissing.h'
        $report.Findings[0].Line | Should Be 7
    }

    It '空文件与纯注释文件不能构成有效扫描通过' {
        Set-Content -LiteralPath (Join-Path $source 'Empty.cpp') -Value ''
        Set-Content -LiteralPath (Join-Path $source 'Comment.cpp') -Value '// 只有说明，没有自有头引用'
        { Test-ProjectOwnedHeaders -WorkspaceRoot $root } | Should Throw '没有有效的自有头文件引用'
    }
}
