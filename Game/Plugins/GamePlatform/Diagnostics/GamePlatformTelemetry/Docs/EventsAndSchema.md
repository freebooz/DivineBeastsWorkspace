# EventsAndSchema（事件与结构）

EventName（事件名）必须使用稳定 Domain.Action.Result（领域.动作.结果）形式，并在 Schema Registry（结构注册表）中预注册。

当前为兼容既有调用暂时预注册 Session.Connect.Succeeded、World.Load.Completed、World.Travel.Completed、Combat.Match.Completed、Commerce.Intent.Created、Commerce.Payment.VerificationResult、Commerce.Fulfillment.Result 及 Telemetry Foundation（遥测基础）事件。长期分层目标是由 Session/World/Commerce/MobaCommon 各自通过 Schema Contributor（结构贡献器）注册本领域语义，GamePlatformTelemetry 本身只拥有 Telemetry.* 基础事件；当前兼容注册不得继续扩张。

Attributes（属性）只支持 String/Int64/Double/Bool（字符串/64位整数/双精度测量/布尔）。属性 Type（类型）、PrivacyClass（隐私等级）和 Event Priority（事件优先级）由 Schema 定义，调用方不能自行降低隐私等级或把高频普通事件提升为 Critical；禁止任意嵌套 JSON Blob。Schema 在第一条运行时记录前允许组合根贡献，开始记录后 Freeze（冻结）为只读。

当前真实落地的是 UE 端 Schema/Privacy/Type 白名单校验。`Shared/Contracts/GamePlatform/Schemas/telemetry-batch.v1.schema.json` 与 Go 后端同构校验当前仓库尚未实现，属于下一阶段跨语言契约任务；在这些文件真实生成并通过测试前，不得声称 UE/Go 双端 Schema 已闭环。