# TestingAndEvidence（测试与证据）

共享模块已有3个 Automation Test源码：AIDefinition结构验证、TargetSelection确定性排序、AIState/AITarget/NativeTag构造。AIDefinition测试已覆盖非法Sight/LoseSight、PreferredRange>AttackRange、PeripheralVision>180，以及启用Hearing但Range=0等结构错误。测试源码存在不等于 UE Automation 已执行。

`Build/Validation/VerifyAI.ps1`已实际执行静态生产门禁：两个模块及宿主类型、共享/服务器依赖隔离、AIServer关键能力、无直接Health修改/自研Navigation/UI/VFX/InputClient/Go实时访问、Client Target无AIServer直链，结果通过。
本轮门禁还明确检查 `RegisterPerceptionStimuliSource（注册感知刺激源）`以及 Development 非AI Target Pawn 存在，避免只有Perception Listener而没有可被Sight发现的目标闭环。

全工作区结构门禁在 AI 依赖落地后实际通过：44插件、76模块、22条项目内依赖边、753项检查，无失败。

BehaviorTree/Blackboard真实资产、UE Automation、Dedicated Server、多客户端、AI数量压力、Editor/Client/Server三目标构建和Cook尚未执行。Network脚本实际返回 `not_executed`，原因 `UE_ROOT/UnrealEditor-Cmd.exe unavailable`；Scalability脚本同样因缺少 UnrealEditor-Cmd 未执行；Build/Cook脚本实际返回 `not_executed`，原因 `UE_ROOT/RunUAT.bat unavailable`。只读环境检查同时确认 `UE_ROOT=null`、RunUAT不存在、UnrealEditor-Cmd不存在、精确Build仍为 `TO_BE_PINNED`。
