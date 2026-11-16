# 真实门禁回归：Cook顺序日志可变，反射头/配置/文件集合不可变；Client-only构建不可充当Editor。
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $taskRoot 'Build/Game/FoundationBuildEvidence.psm1') -Force
$taskModule=Get-Module FoundationBuildEvidence
& $taskModule {
 $a=[pscustomobject]@{Entries=@(@{Path='Game/Source/Test.h';SHA256=('A'*64);Length=10},@{Path='Game/Build/WindowsClient/FileOpenOrder/CookerOpenOrder.log';SHA256=('B'*64);Length=10})}
 $b=[pscustomobject]@{Entries=@(@{Path='Game/Source/Test.h';SHA256=('A'*64);Length=10},@{Path='Game/Build/WindowsClient/FileOpenOrder/CookerOpenOrder.log';SHA256=('C'*64);Length=20})}
 if((Get-FBECookRelevantInputHash $a)-cne(Get-FBECookRelevantInputHash $b)){throw 'Cook文件顺序日志被误判为结构变化'}
 $b.Entries[0].SHA256='D'*64
 if((Get-FBECookRelevantInputHash $a)-ceq(Get-FBECookRelevantInputHash $b)){throw '反射头变化未被拒绝'}
 $b.Entries[0].SHA256='A'*64
 $b.Entries+=@{Path='Game/Config/DefaultGame.ini';SHA256=('E'*64);Length=2}
 if((Get-FBECookRelevantInputHash $a)-ceq(Get-FBECookRelevantInputHash $b)){throw '配置/新增文件变化未被拒绝'}
}
Import-Module (Join-Path $taskRoot 'Build/Game/FoundationCookEvidence.psm1') -Force
$taskRejected=$false
try {Assert-FoundationCookEditorBuildResult -Workspace $taskRoot -EngineRoot 'D:/UnrealEngine-5.8.0-release' -BuildResultPath 'Saved/Validation/FoundationM0/12fe69ab-1402-4557-b688-d76a69277752/Build/result.json'|Out-Null}
catch {$taskRejected=$_.Exception.Message -match 'Editor|完整'; if(!$taskRejected){throw}}
if(!$taskRejected){throw 'Client/Server构建被当成完整Editor证据'}
Write-Output 'COOK_EDITOR_GATE_TESTS_PASS'
