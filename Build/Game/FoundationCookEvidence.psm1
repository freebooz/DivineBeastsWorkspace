# Cook只复用既有完整Editor证据机制，不另设时间戳或仅Client/Server收据的旁路。
Import-Module (Join-Path $PSScriptRoot 'FoundationBuildEvidence.psm1')
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1')
function Assert-FoundationCookEditorBuildResult {
    param([Parameter(Mandatory)][string]$Workspace,[Parameter(Mandatory)][string]$EngineRoot,
        [string]$BuildResultPath,[string[]]$RequiredModules=@('GamePlatformCombat','DivineBeastsAbilitiesRuntime'))
    if([string]::IsNullOrWhiteSpace($BuildResultPath)){
        # 只选择最新成功完整Editor目标，再严格校验当前输入/DLL；不会回退挑选旧凭据。
        $candidates=@(Get-ChildItem -LiteralPath (Join-Path $Workspace 'Saved/Validation/FoundationM0') -Directory|Sort-Object LastWriteTimeUtc -Descending)
        foreach($candidate in $candidates){
            $path=Join-Path $candidate.FullName 'Build/result.json'
            if(!(Test-Path -LiteralPath $path)){continue}
            $record=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
            if($record.Status-eq'Passed'-and$record.EditorBuildEvidenceStatus-eq'Eligible'){$BuildResultPath=$path;break}
        }
    }
    if([string]::IsNullOrWhiteSpace($BuildResultPath)){throw 'Cook缺少成功完整Editor构建证据；请先构建Editor，再重试Cook。'}
    $engine=Get-FoundationEngine $EngineRoot
    Assert-FoundationEditorBuildResult -Workspace $Workspace -Engine $engine -BuildResultPath $BuildResultPath -RequiredModules $RequiredModules -AllowCookFileOrderChanges
}
Export-ModuleMember -Function Assert-FoundationCookEditorBuildResult
