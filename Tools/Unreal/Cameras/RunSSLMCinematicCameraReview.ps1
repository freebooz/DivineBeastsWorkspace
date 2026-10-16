# SSLM电影摄像机独立插件演示入口；使用正式UE5.8源工程，不重新建工程。
# 使用项目既有显式开发演示门禁，不改变正式GameInstance和登录配置。
[CmdletBinding()]
param(
    [switch]$AutoVerify,
    [switch]$CaptureUI
)

$ErrorActionPreference="Stop"
$Workspace=(Resolve-Path (Join-Path $PSScriptRoot "../../..")).Path
$Project=Join-Path $Workspace "Game/DivineBeastsArena.uproject"
$Editor="D:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor.exe"
$Map="/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview"
if(!(Test-Path $Project) -or !(Test-Path $Editor)){
    throw "缺少正式SSLM项目或已锁定的UE5.8编辑器"
}
if(!($Workspace.Replace('\','/').EndsWith("/DivineBeastsWorkspace"))){
    throw "禁止在其它UE工程执行SSLM电影镜头演示"
}
$Log=Join-Path $Workspace ("Game/Saved/Logs/SSLM_CinematicCamera_Standalone_"+(Get-Date -Format yyyyMMdd_HHmmss)+".log")
$ArgsList=[System.Collections.Generic.List[string]]::new()
@(
    $Project,
    $Map,
    "-game",
    "-Windowed",
    "-ResX=1280",
    "-ResY=720",
    "-nosplash",
    "-nosound",
    "-NoLiveCoding",
    "-NoHotReload",
    # 本进程必须同时提供开发演示开关和受控镜头地图路径，Test/Shipping模式仍拒绝。
    "-DBAPluginDemo",
    ("-DBAPluginDemoMap="+$Map),
    ("-abslog="+$Log)
)|ForEach-Object {[void]$ArgsList.Add($_)}
if($AutoVerify){
    @("-SSLMReviewCameraAutoCycle","-benchmark","-fps=30","-seconds=12")|ForEach-Object {[void]$ArgsList.Add($_)}
}
if($CaptureUI){
    # UE5.8中HighResShot会排除UMG；Shot ShowUI才是真正控件截图。
    [void]$ArgsList.Add("-ExecCmds=shot showui")
}
Write-Output "工程：$Project"
Write-Output "关卡：$Map"
Write-Output "验收日志：$Log"
$NativeArgs=$ArgsList.ToArray()
& $Editor @NativeArgs
if($LASTEXITCODE -ne 0){
    throw "UE5.8电影摄像机演示异常退出（代码：$LASTEXITCODE），查看 $Log"
}
if($AutoVerify){
    # 从真实当前会话唯一日志里校验镜头完整切换、光圈、焦点与恢复，不能仅凭进程退出码认定成功。
    if(!(Test-Path $Log)){
        throw "电影镜头自动化验收日志未创建：$Log"
    }
    $LogText=[IO.File]::ReadAllText((Resolve-Path $Log))
    $Markers=@(
        "SSLM_CINE_DEMO_UMG_CREATED",
        "SSLM_CINE_DEMO_SELECTED index=0",
        "SSLM_CINE_DEMO_SELECTED index=1",
        "SSLM_CINE_DEMO_SELECTED index=2",
        "SSLM_CINE_DEMO_SELECTED index=3",
        "SSLM_CINE_DEMO_APERTURE",
        "SSLM_CINE_DEMO_FOCUS",
        "SSLM_CINE_DEMO_RESET",
        "SSLM_CINE_DEMO_AUTOCYCLE_COMPLETED steps=4"
    )
    $Absent=@($Markers|Where-Object {!$LogText.Contains($_)})
    if($Absent.Count -gt 0){
        throw "电影镜头实际功能日志缺失：$($Absent -join ', ')"
    }
    # 本入口声明跳过正式登录，禁止后台初始化的普通Login Widget抢占镜头鼠标焦点。
    if($LogText.Contains("WBP_DBA_UI_Login_C_")){
        throw "电影演示真实运行时出现正式登录界面，免登录开发模式未隔离成功"
    }
    if(!$LogText.Contains("RequestExitWithStatus(0, 0")){
        throw "缺少游戏进程正常退出日志，不能宣称完整运行通过"
    }
    Write-Output "UE5.8电影镜头开发模式自动回归通过：UMG和9项镜头事件存在，无正式登录Widget。"
}
Write-Output "UE5.8电影摄像机演示退出；屏幕UI点击及真实画质仍需人工验收。"