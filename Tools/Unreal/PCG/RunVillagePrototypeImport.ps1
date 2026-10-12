#requires -Version 5.1
<#
.SYNOPSIS
神兽联盟Village六类PCG原型网格的独立UE5.8真实导入/反射回读入口。
.DESCRIPTION
Tech Mesh（技术模型）不等于3A正式美术。默认只读inspect；
-Apply导入到项目WorldPack Development目录（正式Cook排除），
-Verify仅在独立UE进程回读已有StaticMesh。拒绝已有同名资源和编辑器/UBT并发。
超时只终止本脚本创建的UnrealEditor-Cmd进程，禁止Kill其他Editor。
#>
[CmdletBinding()]
param([string]$EngineRoot = $env:UE_ROOT,
      [ValidateSet('Inspect','Apply','Verify','ReviewApply','ReviewVerify')][string]$Mode='Inspect')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$source=Join-Path $PSScriptRoot 'GenerateVillagePrototypeMeshes.py'
$author=Join-Path $PSScriptRoot 'ImportVillageTechnicalPrototypes.py'
$review=Join-Path $PSScriptRoot 'AuthorVillagePrototypeReviewMap.py'
$manifest=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/PCG/TechnicalPrototype_v1/VillagePCGTechnicalSourceManifest_v1.json'
$project=Join-Path $root 'Game/DivineBeastsArena.uproject'
$run=Join-Path $root ('Saved/Validation/GamePlatformPCG/VillagePrototypeImport/'+[guid]::NewGuid().ToString())
New-Item -Path $run -ItemType Directory -Force|Out-Null

try {
    foreach($file in @($source,$author,$review,$manifest,$project)){
        if(!(Test-Path -LiteralPath $file)){throw ('技术模型源或真实工程缺失：'+$file)}
    }
    & python $source --verify
    if($LASTEXITCODE -ne 0){throw '六类技术源OBJ的固定种子/几何/SHA-256复验失败'}
    if($Mode -eq 'Inspect'){
        & python $author
        if($LASTEXITCODE -ne 0){throw '技术导入清单只读审核失败'}
        Write-Output 'TECHNICAL_SOURCE_INSPECT_PASS_NO_EDITOR_ASSETS_CREATED'
        return
    }
    if(!$EngineRoot){throw '必须显式传递UE5.8引擎根目录'}
    $engine=(Resolve-Path -LiteralPath $EngineRoot).Path
    $version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8|ConvertFrom-Json
    if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw '仅接受固定UE5.8引擎'}
    $editor=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    if(!(Test-Path $editor)){throw '真实UnrealEditor-Cmd.exe不存在'}
    if(@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count){
        throw '现有UE编辑器正在使用资源，不抢占其所有权'
    }
    if(@(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" |
          Where-Object{$_.CommandLine -match 'UnrealBuildTool'}).Count){
        throw 'UE构建正在使用共享中间文件，拒绝与其它会话争用'
    }
    $roles=@('ForestCanopy','RockScatter','AgriCrop','FieldFence','FarmGate','WoodBridge')
    $pack=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Development/PCG/Meshes'
    $reviewFile=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Development/PCG/Maps/L_DBA_PCG_TechMeshReview.umap'
    $isReview=$Mode -in @('ReviewApply','ReviewVerify')
    if($isReview){
        foreach($role in $roles){
            $tech=Join-Path $pack ('SM_DBA_PCG_'+$role+'_Tech.uasset')
            if(!(Test-Path -LiteralPath $tech)){throw ('真实技术StaticMesh尚未导入，不能审查地图：'+$tech)}
        }
        if($Mode -eq 'ReviewApply' -and (Test-Path $reviewFile)){throw '技术审查地图已存在，不能覆盖'}
        if($Mode -eq 'ReviewVerify' -and !(Test-Path $reviewFile)){throw '技术审查地图尚未创建，不能回读'}
    }
    if($Mode -eq 'Apply'){
        foreach($role in $roles){
            $f=Join-Path $pack ('SM_DBA_PCG_'+$role+'_Tech.uasset')
            if(Test-Path -LiteralPath $f){throw ('技术UE资产已存在，禁止覆盖：'+$f)}
        }
    }
    $sourceRows=(Get-Content $manifest -Raw -Encoding UTF8|ConvertFrom-Json).meshSources
    $hashes=@{}
    foreach($row in $sourceRows){
        $path=Join-Path (Split-Path $manifest -Parent) $row.relativePath
        $hashes[$path]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
    $envOld=$env:VILLAGE_PCG_MESH_IMPORT_MODE
    $oldReview=$env:VILLAGE_PCG_TECH_REVIEW_MODE
    try {
        $env:VILLAGE_PCG_MESH_IMPORT_MODE=if($Mode -eq 'Apply'){'apply'}else{'verify'}
        $env:VILLAGE_PCG_TECH_REVIEW_MODE=if($Mode -eq 'ReviewApply'){'apply'}else{'verify'}
        $selectedScript=if($isReview){$review}else{$author}
        $output=Join-Path $run ($Mode+'.stdout.log')
        $errorLog=Join-Path $run ($Mode+'.stderr.log')
        $args=@($project,("-ExecutePythonScript="+$selectedScript),
            '-VillagePCGTechMesh','-EnablePlugins=PythonScriptPlugin,DBAWorldPack_Village',
            '-DisablePlugins=Monolith','-NoCompile','-unattended','-nop4','-nosplash',
            '-nosound','-nullrhi','-stdout')
        $process=Start-Process -FilePath $editor -ArgumentList $args -PassThru -RedirectStandardOutput $output -RedirectStandardError $errorLog
        if(!$process.WaitForExit(420000)){
            $process.Kill()
            [void]$process.WaitForExit(5000)
            throw '技术模型UE编辑器导入超过420秒，已终止本次专有进程；没有停止别人的Editor'
        }
        $process.Refresh()
        foreach($path in $hashes.Keys){
            if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hashes[$path]){
                throw ('技术OBJ源在UE导入过程中被意外修改：'+$path)
            }
        }
        $log=Get-Content $output -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        $marker=if($isReview){
            if($Mode -eq 'ReviewApply'){'UE_PCG_SIX_TECH_MESHES_REAL_MAP_SAVED'}
            else{'UE_PCG_SIX_REAL_STATIC_MESHES_MAP_REOPEN_OK'}
        }else{'UE_VILLAGE_PCG_TECH_MESHES_REAL_IMPORT_OR_VERIFY_SUCCESS'}
        if($process.ExitCode -ne 0 -or !$log.Contains($marker)){
            throw ('独立UE没有返回六网格完成标志：'+$process.ExitCode+' '+($log.Substring([Math]::Max(0,$log.Length-1500))))
        }
        if(!$isReview){
            foreach($role in $roles){
                if(!$log.Contains('UE_VILLAGE_PCG_TECH_MESH_ASSET_OK /DBAWorldPack_Village/Development/PCG/Meshes/SM_DBA_PCG_'+$role+'_Tech')){
                    throw ('UE反射回读缺少准确的技术静态网格：'+$role)
                }
            }
        }
        if($isReview -and !(Test-Path -LiteralPath $reviewFile)){
            throw 'UE没有保存真实技术审查.umap，拒绝把日志误认为资产成功'
        }
        Write-Output ('VILLAGE_TECHNICAL_UE_'+$Mode.ToUpperInvariant()+'_PASS')
        Write-Output 'TECHNICAL_ONLY_NOT_FINAL_AAA_ART_NOT_AUTOMATIC_GAMEPLAY_AUTHORITY'
    }
    finally {
        $env:VILLAGE_PCG_MESH_IMPORT_MODE=$envOld
        $env:VILLAGE_PCG_TECH_REVIEW_MODE=$oldReview
    }
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
finally{
    Write-Output ('技术原型导入证据目录：'+$run)
}
