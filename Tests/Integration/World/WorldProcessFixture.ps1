#requires -Version 7.0
<# 仅供脚本进程安全测试：写测试日志，不启动UE，不创建资产，不是World运行证据。 #>
param([string]$LogPath,[string]$RunId,[ValidateSet('Ready','WrongPid','Crash')][string]$Mode='Ready')
$generation=[guid]::NewGuid().ToString('D')
$processIdentity=if($Mode -eq 'WrongPid'){$PID+1}else{$PID}
$lines=@('Initialized','DefinitionLoaded','MapMatched','RegionsReady','WorldReady') | ForEach-Object {"LogDBAWorld: WorldValidation RunId=$RunId ProcessId=$processIdentity Scenario=Foundation Event=$_ Generation=$generation"}
[IO.File]::WriteAllLines($LogPath,$lines,[Text.UTF8Encoding]::new($false))
if($Mode -eq 'Crash'){exit 17}
Start-Sleep -Seconds 30
