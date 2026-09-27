Describe '实际工程默认配置完整性' {
    $workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    Import-Module (Join-Path $PSScriptRoot 'DesignBaselineAudit.psm1') -Force

    It '交付工程本身满足八项默认配置门槛，而不只验证完整测试夹具' {
        $report = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot
        $configErrors = @($report.Errors | Where-Object { $_ -match '默认配置|DefaultEngine' })
        ($configErrors -join "`n") | Should BeNullOrEmpty
        $report.RequiredConfigCount | Should Be 8
        $report.ConfigCount | Should BeGreaterThan 7
    }

    It '自动保存归专用用户设置层，Editor层只放项目蓝图检查设置' {
        $editor = Get-Content -LiteralPath (Join-Path $workspaceRoot 'Game/Config/DefaultEditor.ini') -Raw
        $editor | Should Not Match 'EditorLoadingSavingSettings'
        $editor | Should Match 'BlueprintEditorProjectSettings'
        $editor | Should Match 'bValidateUnloadedSoftActorReferences=True'
        $userConfig = Join-Path $workspaceRoot 'Game/Config/DefaultEditorPerProjectUserSettings.ini'
        (Test-Path -LiteralPath $userConfig -PathType Leaf) | Should Be $true
        if (Test-Path -LiteralPath $userConfig) {
            $content = Get-Content -LiteralPath $userConfig -Raw
            $content | Should Match 'EditorLoadingSavingSettings'
            $content | Should Match 'bAutoSaveEnable=True'
            $content | Should Match 'AutoSaveTimeMinutes=10'
        }
    }
}
