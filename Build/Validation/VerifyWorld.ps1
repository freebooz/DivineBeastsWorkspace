#requires -Version 7.0
<#
.SYNOPSIS
World真实原生Debug/Release测试和可选Editor/Client/Server构建门禁；不代表完整世界验收。
.DESCRIPTION
复用FoundationTools进程、UE5.8检查及引擎构建锁。每轮新RunId，仅Saved内生成产物。
Runtime/Cook/Stage/MultiPIE/人工核验本入口未执行，必须保留NotExecuted，整体不能全通过。
不存在的工具/前置返回2；已启动工具的非零退出码保留，状态为Failed（包括外部退出2）。
.EXAMPLE
./Build/Validation/VerifyWorld.ps1 -NativeTests
.EXAMPLE
./Build/Validation/VerifyWorld.ps1 -Targets Editor,Client,Server -EngineRoot <UE5.8根目录>
#>
param([switch]$NativeTests,[ValidateSet('Editor','Client','Server')][string[]]$Targets=@(),[string]$EngineRoot=$env:UE_ROOT,[string]$CMake='cmake',[string]$NativeGenerator,[ValidateRange(1,86400)][int]$TimeoutSeconds=180,[guid]$RunId=[guid]::NewGuid())
. (Join-Path $PSScriptRoot '../../Tests/Integration/World/WorldValidationTools.ps1')
$context=New-WorldValidationContext $RunId VerifyWorld
$cases=[ordered]@{}
foreach($name in @('NativeDebug','NativeRelease','BuildEditor','BuildClient','BuildServer','RuntimeFoundation','RuntimeSession','RuntimePartition','Cook','Stage','MultiPIE','ManualReview')){$cases[$name]=New-WorldCase $name}
$cases.RuntimeSession.Message='Session公开快照和真实客户端/专用服务器适配缺失；匹配及错误WorldId未执行。'
foreach($name in @('RuntimeFoundation','RuntimePartition','Cook','Stage','MultiPIE','ManualReview')){$cases[$name].Message='本门禁不执行此验收；必须另行获得本轮真实证据，不能由原生或编译结果代替。'}
$steps=[Collections.Generic.List[object]]::new(); $details=@{BinaryHashes=@{};Scope='原生算法/可选正式目标构建，不是UE运行、资产、联网或烘焙验收。'}
$sourceSnapshot=Get-WorldSourceSnapshot $context.Workspace
$details.BuildEvidenceVersion=2; $details.Workspace=$context.Workspace
$details.SourceFingerprint=$sourceSnapshot.Fingerprint; $details.SourceInventory=$sourceSnapshot.Files; $details.SourceFingerprintAlgorithm=$sourceSnapshot.Algorithm
$details.EditorBinaryHashes=@{}
$details.BuildEvidenceScope='限定主工程及World/Loading/Core/Data源码与配置；成功Editor记录五DLL，不证明其余依赖闭包或引擎版本二进制完全一致。'
if($NativeTests){
    try {
        $source=Join-Path $context.Workspace 'Game/Plugins/GameFoundation/Gameplay/GamePlatformWorld/Tests'
        Assert-FoundationFile (Join-Path $source 'CMakeLists.txt')
        $command=Get-Command $CMake -CommandType Application -ErrorAction SilentlyContinue
        if(-not $command){throw [IO.FileNotFoundException]::new('未找到CMake；没有下载或替换工具链。')}
        $cmakePath=$command.Source; $ctest=Join-Path (Split-Path $cmakePath) 'ctest.exe'; Assert-FoundationFile $ctest
        $native=Join-Path $context.Directory 'Native'
        $arguments=@('-S',$source,'-B',$native); if($NativeGenerator){$arguments+=@('-G',$NativeGenerator)}
        $configure=Invoke-WorldTool $context Configure $cmakePath $arguments $TimeoutSeconds; $steps.Add($configure)
        if($configure.ExitCode -ne 0){
            $cases.NativeDebug=New-WorldCase NativeDebug Failed $configure.ExitCode '真实CMake配置失败；后续构建和CTest未运行。' @($configure.Evidence)
            $cases.NativeRelease.Message='配置失败，未执行Release。'
        } else {
            foreach($config in @('Debug','Release')){
                $build=Invoke-WorldTool $context "Build-$config" $cmakePath @('--build',$native,'--config',$config) $TimeoutSeconds; $steps.Add($build)
                if($build.ExitCode -ne 0){$cases["Native$config"]=New-WorldCase "Native$config" Failed $build.ExitCode '真实原生构建失败，CTest未运行。' @($build.Evidence); continue}
                $test=Invoke-WorldTool $context "Test-$config" $ctest @('--test-dir',$native,'-C',$config,'--output-on-failure','--no-tests=error','-V','--output-junit',(Join-Path $context.Directory "CTest-$config.xml")) $TimeoutSeconds; $steps.Add($test)
                $cases["Native$config"]=New-WorldCase "Native$config" $test.Status $test.ExitCode '生产算法CTest结果；不等于UE生命周期/资源行为验证。' @($build.Evidence,$test.Evidence)
                if($test.ExitCode -eq 0){
                    try {Assert-WorldCTestReport ([IO.File]::ReadAllText((Join-Path $context.Directory "CTest-$config.xml")))}
                    catch {$cases["Native$config"]=New-WorldCase "Native$config" Failed 1 $_.Exception.Message @($test.Evidence)}
                }
            }
        }
    } catch [IO.FileNotFoundException] {
        foreach($name in @('NativeDebug','NativeRelease')){if($cases[$name].Status -eq 'NotExecuted'){$cases[$name]=New-WorldCase $name NotExecuted 2 $_.Exception.Message}}
    } catch {$cases.NativeDebug=New-WorldCase NativeDebug Failed 1 $_.Exception.Message}
}
foreach($target in ($Targets | Select-Object -Unique)){
    $lock=$null
    try {
        $engine=Get-FoundationEngine $EngineRoot; $details.EngineVersion=$engine.Version
        $tool=Get-FoundationManagedTool $engine UnrealBuildTool
        Assert-FoundationFile $context.Project
        Assert-FoundationFile (Join-Path $context.Workspace "Game/Source/DivineBeastsArena$target.Target.cs")
        $lock=Enter-FoundationEngineLock $engine.Root
        $args=@($tool.Dll,"DivineBeastsArena$target",'Win64','Development',"-Project=$($context.Project)",'-MaxParallelActions=2','-NoSharedPCH','-NoHotReloadFromIDE',"-Log=$(Join-Path $context.Directory "UBT-$target.log")")
        $step=Invoke-WorldTool -Context $context -Name "UE-$target" -Executable $tool.Executable -Arguments $args -TimeoutSeconds $TimeoutSeconds -Environment $tool.Environment -WorkingDirectory (Join-Path $engine.Root 'Engine/Source'); $steps.Add($step)
        $cases["Build$target"]=New-WorldCase "Build$target" $step.Status $step.ExitCode '真实UBT Win64 Development；不移动空描述文件、不禁用扫描、不旁路项目。' @($step.Evidence)
        if($step.ExitCode -eq 0){
            $binaryName=if($target -eq 'Editor'){'UnrealEditor-DivineBeastsArena.dll'}else{"DivineBeastsArena$target.exe"}
            $binary=Join-Path $context.Workspace "Game/Binaries/Win64/$binaryName"
            if(-not (Test-Path -LiteralPath $binary -PathType Leaf)){throw 'UBT返回0但没有目标二进制，不能提供可运行构建证据。'}
            $details.BinaryHashes[$target]=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
            if($target -eq 'Editor'){
                # UBT返回成功后缺DLL属于已执行验证失败，不是未执行；不得留部分模块哈希。
                try {$details.EditorBinaryHashes=Get-WorldEditorBinaryHashes $context.Workspace}
                catch {throw [InvalidOperationException]::new($_.Exception.Message)}
            }
            if((Get-WorldSourceSnapshot $context.Workspace).Fingerprint -cne $details.SourceFingerprint){throw '构建期间源码指纹变化，不能将此构建作为当前源码证据。'}
        }
    } catch [IO.FileNotFoundException] {$cases["Build$target"]=New-WorldCase "Build$target" NotExecuted 2 $_.Exception.Message}
      catch {$cases["Build$target"]=New-WorldCase "Build$target" Failed 1 $_.Exception.Message}
      finally {if($lock){$lock.ReleaseMutex();$lock.Dispose()}}
}
$details.SourceFingerprintAfter=(Get-WorldSourceSnapshot $context.Workspace).Fingerprint
if($details.SourceFingerprintAfter -cne $details.SourceFingerprint){
    $details.SourceChangedDuringRun=$true
    foreach($target in @('Editor','Client','Server')){if($cases["Build$target"].Status -eq 'Passed'){$cases["Build$target"]=New-WorldCase "Build$target" Failed 1 '本轮源码变更，需稳定快照后重新构建。'}}
    $details.EditorBinaryHashes=@{}
}
$verdict=Write-WorldReport $context @($cases.Values) @($steps.ToArray()) $details
exit $verdict.ExitCode
