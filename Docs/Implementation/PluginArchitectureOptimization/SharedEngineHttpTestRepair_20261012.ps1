#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$WorkspaceRoot,
    [Parameter(Mandatory)][string]$EngineRoot,
    [Parameter(Mandatory)][string]$EvidenceRoot,
    [switch]$Execute,
    [string]$RestoreRun
)
$ErrorActionPreference = 'Stop'
$utf8 = [Text.UTF8Encoding]::new($false, $true)
$workspace = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $WorkspaceRoot).Path)
$engine = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $EngineRoot).Path)
$evidence = [IO.Path]::GetFullPath($EvidenceRoot)
$allowedEvidence = [IO.Path]::GetFullPath((Join-Path $workspace 'Saved/Validation')) + [IO.Path]::DirectorySeparatorChar
if (-not ($evidence + [IO.Path]::DirectorySeparatorChar).StartsWith($allowedEvidence, [StringComparison]::OrdinalIgnoreCase)) { throw 'EvidenceRoot must be beneath this WorkspaceRoot/Saved/Validation.' }
$testRel = 'Engine/Source/Runtime/Online/HTTP/Private/Tests/HttpRedirectPolicyTests.cpp'
$test = Join-Path $engine $testRel
$patchDir = Join-Path $workspace 'Build/Engine/Patches'
$patch = Join-Path $patchDir 'UE5.8-HttpRedirectPolicy.patch'
$applier = Join-Path $patchDir 'Apply-UE58HttpRedirectPolicy.ps1'
$hashFile = Join-Path $patchDir 'UE5.8-HttpRedirectPolicy.hashes.json'
$oldTest = '26E39B9061D0526B47DA802CB6763EF09434183AC5C06278E107C8310FA1C672'
$newTest = '2861D419369AA45D62AC9FE8AC9823A9CCF706A7D9ED9B204E51F6C6AF26225B'
$rawPatch = 'A49EAD79D734E793714F47649B194F4B79F2F53B5779B6DDE5C07EF7F7A8F861'
$canonicalPatch = '8F78CDD1FB4E8E02720F90519F7154C8C546CB3C00BC345B53E2B34E885C7F35'
$immutable = [ordered]@{
    $applier = '4E30495178E5F96AC68C8B6AF3ED8BDF31CB57B52797FAB52042F3B3A9170F86'
    $hashFile = '73904A8F58011D2134EF5FA0FA8BD42D14BC2AA7BBD65E69ABAD5EF357C5C751'
    (Join-Path $engine 'Engine/Source/Runtime/Online/HTTP/Public/Interfaces/IHttpRequest.h') = '45A63D22608E23C83B32EA2E7CD6AAD3429E906643A038BE83E49614ED9B07ED'
    (Join-Path $engine 'Engine/Source/Runtime/Online/HTTP/Private/Curl/CurlHttp.h') = 'BD5836507C72B4A014E5E1CB3965F7C7D31EF98DA41588E6AC88239F8C25A019'
    (Join-Path $engine 'Engine/Source/Runtime/Online/HTTP/Private/Curl/CurlHttp.cpp') = 'AE06C3EF230E354DA37CCF58584BF18BC600860E3478A5BC93EAF9B21BC4AECC'
}
function Get-BytesHash([byte[]]$Bytes) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($algorithm.ComputeHash($Bytes)) } finally { $algorithm.Dispose() }
}
function Assert-NoReparse([string]$Path) {
    for ($current = $Path; $current; $current = Split-Path $current -Parent) {
        if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Reparse path refused: $current" }
    }
}
function Assert-ImmutableInputs {
    foreach ($entry in $immutable.GetEnumerator()) {
        Assert-NoReparse $entry.Key
        if ((Get-FileHash -LiteralPath $entry.Key -Algorithm SHA256).Hash -cne $entry.Value) { throw "Unreviewed immutable input: $($entry.Key)" }
    }
}
function Read-StreamBytes([IO.FileStream]$Stream) {
    $data = [byte[]]::new([int]$Stream.Length); $Stream.Position = 0; $offset = 0
    while ($offset -lt $data.Length) { $n = $Stream.Read($data, $offset, $data.Length-$offset); if ($n -eq 0) { throw 'Unexpected EOF.' }; $offset += $n }
    return ,$data
}
function Write-Guarded([string]$Path, [string]$BeforeSHA, [byte[]]$Bytes, [string]$AfterSHA) {
    if ((Get-BytesHash $Bytes) -cne $AfterSHA) { throw 'Candidate bytes do not match approved SHA.' }
    Assert-NoReparse $Path
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $before = Read-StreamBytes $stream
        if ((Get-BytesHash $before) -cne $BeforeSHA) { throw "Fresh pre-write SHA mismatch: $Path" }
        try {
            $stream.Position=0; $stream.SetLength($Bytes.Length); $stream.Write($Bytes,0,$Bytes.Length); $stream.Flush($true)
            if ((Get-BytesHash (Read-StreamBytes $stream)) -cne $AfterSHA) { throw "Post-write SHA mismatch: $Path" }
        } catch {
            $writeError = $_
            $stream.Position=0; $stream.SetLength($before.Length); $stream.Write($before,0,$before.Length); $stream.Flush($true)
            if ((Get-BytesHash (Read-StreamBytes $stream)) -cne $BeforeSHA) { throw "Per-file backup restoration failed: $Path; original: $writeError" }
            throw $writeError
        }
    } finally { $stream.Dispose() }
}
function Restore-Backups([string]$Directory, $Record) {
    $errors = @()
    foreach ($item in @(
        @{Path=$test; File='test.before.cpp'; Before=[string]$Record.TestBeforeSHA256; After=$newTest},
        @{Path=$patch; File='patch.before.patch'; Before=[string]$Record.PatchBeforeSHA256; After=$canonicalPatch}
    )) {
        try {
            $bytes = [IO.File]::ReadAllBytes((Join-Path $Directory $item.File))
            if ((Get-BytesHash $bytes) -cne $item.Before) { throw "Backup hash mismatch: $($item.File)" }
            $current = (Get-FileHash -LiteralPath $item.Path -Algorithm SHA256).Hash
            if ($current -ceq $item.Before) { continue }
            if ($current -cne $item.After) { throw "Concurrent/unknown bytes preserved: $($item.Path)" }
            Write-Guarded $item.Path $item.After $bytes $item.Before
        } catch { $errors += $_.Exception.Message }
    }
    return ,$errors
}
Assert-NoReparse $test; Assert-NoReparse $patch; Assert-NoReparse $evidence
Assert-ImmutableInputs
$version = Get-Content -LiteralPath (Join-Path $engine 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8 -or $version.PatchVersion -ne 0) { throw 'Only the reviewed UE 5.8.0 is supported.' }
if ($RestoreRun) {
    $restore = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $RestoreRun).Path)
    if (-not ($restore + [IO.Path]::DirectorySeparatorChar).StartsWith($evidence + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'RestoreRun must be inside EvidenceRoot.' }
    $record = Get-Content -LiteralPath (Join-Path $restore 'before.json') -Raw | ConvertFrom-Json
    if ($record.Workspace -cne $workspace -or $record.EngineRoot -cne $engine -or $record.TestBeforeSHA256 -cnotin @($oldTest,$newTest) -or $record.PatchBeforeSHA256 -cnotin @($rawPatch,$canonicalPatch)) { throw 'Restore identity or baseline mismatch.' }
    if (-not $Execute) { [pscustomobject]@{Status='RollbackPlanOnly';RestoreRun=$restore;SharedTargetsWritten=$false} | ConvertTo-Json; return }
    $errors = Restore-Backups $restore $record
    $out = [ordered]@{Status=$(if($errors.Count){'ManualReviewRequired'}else{'RolledBack'});RestoreRun=$restore;Errors=@($errors);UEExecuted=$false;UBTExecuted=$false}
    $out | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $restore ('rollback-' + [guid]::NewGuid().ToString('D') + '.json')) -Encoding utf8
    if ($errors.Count) { throw ($errors -join '; ') }
    $out | ConvertTo-Json -Depth 4; return
}
$testBefore = [IO.File]::ReadAllBytes($test); $patchBefore = [IO.File]::ReadAllBytes($patch)
$testBeforeSHA = Get-BytesHash $testBefore; $patchBeforeSHA = Get-BytesHash $patchBefore
if ($testBeforeSHA -cnotin @($oldTest,$newTest) -or $patchBeforeSHA -cnotin @($rawPatch,$canonicalPatch)) { throw 'Unknown test or patch material bytes; no writes.' }
$patchAfter = $utf8.GetBytes($utf8.GetString($patchBefore).Replace("`r`n","`n"))
if ((Get-BytesHash $patchAfter) -cne $canonicalPatch) { throw 'LF material does not match the existing registered digest.' }
$candidate = $testBefore
if ($testBeforeSHA -ceq $oldTest) {
    $candidateText = $utf8.GetString($testBefore)
    $lineEnd = if ($candidateText.Contains("`r`n")) { "`r`n" } else { "`n" }
    $accessBefore = "public:" + $lineEnd + "`tbool SetupForTest() { return SetupRequest(); }"
    $accessAfter = "public:" + $lineEnd + "`tusing FHttpRequestCommon::CleanupRequest; // 仅测试类公开基类虚函数，调用仍分派至Curl私有覆盖。" + $lineEnd + "`tbool SetupForTest() { return SetupRequest(); }"
    $flagsBefore = 'EAutomationTestFlags::ApplicationContextMask'
    if ([regex]::Matches($candidateText, [regex]::Escape($accessBefore)).Count -ne 1 -or [regex]::Matches($candidateText, [regex]::Escape($flagsBefore)).Count -ne 2) { throw 'Reviewed three-site source shape changed.' }
    $candidateText = $candidateText.Replace($accessBefore, $accessAfter).Replace($flagsBefore, 'EAutomationTestFlags_ApplicationContextMask')
    $candidate = $utf8.GetBytes($candidateText)
}
if ((Get-BytesHash $candidate) -cne $newTest) { throw 'Derived candidate bytes do not match the registered test digest.' }
$proposal = Join-Path $workspace 'Docs/Implementation/PluginArchitectureOptimization/SharedEngineHttpTestRepairProposal_20261012.patch'
if ((Get-FileHash -LiteralPath $proposal -Algorithm SHA256).Hash -cne '5DB9C9B59279A1CBED1386DD39792E7B273ED9F02553A5DED0E124BCCF5FD469') { throw 'Reviewed three-site proposal patch changed.' }
if (-not $Execute) { [pscustomobject]@{Status='PlanOnly';Workspace=$workspace;EngineRoot=$engine;TestBeforeSHA256=$testBeforeSHA;TestAfterSHA256=$newTest;PatchBeforeSHA256=$patchBeforeSHA;PatchAfterSHA256=$canonicalPatch;ExistingGateChanged=$false;TargetsWritten=$false} | ConvertTo-Json; return }
$run = Join-Path $evidence ([guid]::NewGuid().ToString('D'))
$null = New-Item -ItemType Directory -Path $run -ErrorAction Stop
[IO.File]::WriteAllBytes((Join-Path $run 'test.before.cpp'),$testBefore)
[IO.File]::WriteAllBytes((Join-Path $run 'patch.before.patch'),$patchBefore)
$record=[ordered]@{Workspace=$workspace;EngineRoot=$engine;PreparationRunId='b34b20fc-6554-464e-b585-dbffec8e9123';TestBeforeSHA256=$testBeforeSHA;PatchBeforeSHA256=$patchBeforeSHA;ExpectedTestAfterSHA256=$newTest;ExpectedPatchAfterSHA256=$canonicalPatch;StartedUTC=[DateTime]::UtcNow.ToString('o')}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'before.json') -Encoding utf8
try {
    if ($patchBeforeSHA -cne $canonicalPatch) { Write-Guarded $patch $patchBeforeSHA $patchAfter $canonicalPatch }
    if ($testBeforeSHA -cne $newTest) { Write-Guarded $test $testBeforeSHA $candidate $newTest }
    Assert-ImmutableInputs
    $gateOutput = & $applier -EngineRoot $engine -CheckOnly -EvidenceRoot $run
    $gateOutput | Out-String | Set-Content -LiteralPath (Join-Path $run 'existing-applier-checkonly.log') -Encoding utf8
    if (($gateOutput | Out-String) -notmatch 'AlreadyApplied') { throw 'Existing unchanged applier did not confirm AlreadyApplied.' }
    if ((Get-FileHash -LiteralPath $test -Algorithm SHA256).Hash -cne $newTest -or (Get-FileHash -LiteralPath $patch -Algorithm SHA256).Hash -cne $canonicalPatch) { throw 'Final approved SHA check failed.' }
    $result=[ordered]@{Status='AppliedAndGateVerified';RunDirectory=$run;TestSHA256=$newTest;PatchSHA256=$canonicalPatch;ExistingGateChanged=$false;ProductionFilesUnchanged=$true;UEExecuted=$false;UBTExecuted=$false;CookExecuted=$false;CompletedUTC=[DateTime]::UtcNow.ToString('o')}
} catch {
    $failure=$_.Exception.Message; $errors=Restore-Backups $run ([pscustomobject]$record)
    $result=[ordered]@{Status=$(if($errors.Count){'ManualReviewRequired'}else{'FailedAndRolledBack'});RunDirectory=$run;Failure=$failure;RollbackErrors=@($errors);UEExecuted=$false;UBTExecuted=$false}
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
    throw ($failure + $(if($errors.Count){'; rollback: '+($errors -join '; ')}))
}
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
$result | ConvertTo-Json -Depth 4
