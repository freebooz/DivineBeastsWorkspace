# TimeOvertimeAndSuddenDeath（时间、加时与突然死亡）

TimeLimitSeconds当前为0（未配置），不会继承平台900秒开发默认。

客户端时间显示应使用平台GameState Server World Time（服务器世界时间）与Deadline，不每秒RPC。

OvertimeState当前NotConfigured。
SuddenDeathState当前NotConfigured。

Release前必须明确：
- 正式TimeLimit。
- Overtime是Unsupported还是Supported。
- Supported时的OvertimePolicyId。
- SuddenDeath是Unsupported还是Supported。

未确认前Production Validation失败。
