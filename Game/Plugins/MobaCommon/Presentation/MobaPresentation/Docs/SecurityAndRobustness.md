# SecurityAndRobustness（安全与健壮性）

Presentation Payload即使来自本地也不能触发任意文件路径加载、任意Class Spawn、URL请求、SQL或脚本执行。

MobaPresentation只通过受控Semantic Tag和平台Presentation Provider路由资源。

FactId + Revision + WorldGeneration + AvatarGeneration用于去重和过期保护；预测事实确认时不二次播放。ProviderMissing、无资源匹配、World Stale等情况采取日志/诊断和Safe Drop（安全丢弃），Gameplay继续。

客户端不能通过表现请求修改Arena分数、胜负、Combat伤害、Ability状态或后端业务事实。
