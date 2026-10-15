<#
职责：本地开发专服的PlayerData环境装配，供Village与MainArena启动入口共同调用。
边界：仅设置指定ProcessStartInfo，不读取账号、不修改父进程/系统环境；不为缺失地址提供默认值。
生命周期：可选启动前就绪检查失败即抛错，调用方不得继续创建专服；不在UI或游戏运行期间轮询。
#>
function Set-LocalPlayerDataEnvironment {
    <# 将显式回环HTTP(S)资料根地址传给子进程；CheckReady只在启动前检查一次健康端点。
       参数StartInfo为调用方独占的进程配置；无输出。非法地址或健康失败抛出不含凭据的错误。 #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][Diagnostics.ProcessStartInfo]$StartInfo,
        [AllowEmptyString()][AllowNull()][string]$PlayerDataBaseUrl,[switch]$CheckReady)
    $taskUri=$null
    # URI解析器会容忍首尾空白，但UE按原始环境字符串校验；这里明确拒绝，防止“预检通过、角色请求禁用”。
    if([string]::IsNullOrWhiteSpace($PlayerDataBaseUrl)-or$PlayerDataBaseUrl-cne$PlayerDataBaseUrl.Trim()-or$PlayerDataBaseUrl-match'[\r\n\\]'-or
        $PlayerDataBaseUrl-notmatch'^https?://'-or
        ![uri]::TryCreate($PlayerDataBaseUrl,[UriKind]::Absolute,[ref]$taskUri)-or
        $taskUri.Scheme-notin@('http','https')-or!$taskUri.IsLoopback-or$taskUri.UserInfo-or
        $taskUri.Query-or$taskUri.Fragment-or$taskUri.AbsolutePath-ne'/'){
        throw '必须显式提供无凭据、查询、片段及子路径的本地PlayerData HTTP(S)根地址。'
    }
    $taskBase=$PlayerDataBaseUrl.TrimEnd('/')
    if($CheckReady){
        try{
            $taskResponse=Invoke-WebRequest -Uri ($taskBase+'/health/ready') -TimeoutSec 5
            if($taskResponse.StatusCode-ne200){throw 'NotReady'}
        }catch{throw 'PlayerData启动前检查失败，未启动专服；请检查指定资料服务。'}
    }
    $StartInfo.Environment['PLAYERDATA_BASE_URL']=$taskBase
}
Export-ModuleMember -Function Set-LocalPlayerDataEnvironment
