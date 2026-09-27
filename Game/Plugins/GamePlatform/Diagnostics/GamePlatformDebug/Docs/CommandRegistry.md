# CommandRegistry（命令注册表）

核心命令使用 UE5.8 IConsoleManager（控制台管理器），不以旧 Exec（执行接口）作为平台命令体系。

只读命令：
gp.Debug.Status、gp.Debug.World、gp.Debug.Player、gp.Debug.Character、gp.Debug.Ability、gp.Debug.Combat、gp.Debug.AI、gp.Debug.Navigation、gp.Debug.Online、gp.Debug.Session、gp.Debug.Loading、gp.Debug.Telemetry、gp.Debug.Network。

低风险开发期命令：
- gp.Debug.Category <ProviderId> <0|1>（启停状态提供者）。
- gp.Debug.Refresh（推进快照修订号，不修改玩法）。
- gp.Debug.Trace.Start / Stop / Status（桥接 UE Trace，虚幻追踪）。
- gp.Debug.Telemetry.Flush（遥测尽力刷新）。
- gp.Debug.Panel（客户端调试面板开关）。

gp.Debug.AllowMutating（允许低风险可变调试命令）默认 0；Test（测试）构建因此默认只读。必须显式设为 1 才执行上述低风险命令。

每条命令均登记 Name/Help/Category/Args/ReadOnly-or-Mutating/RequiredPrivilege/AllowedBuilds（名称/帮助/分类/参数/只读或可变/所需权限/允许构建）。