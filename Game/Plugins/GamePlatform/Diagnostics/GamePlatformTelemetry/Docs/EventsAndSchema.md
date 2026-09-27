# EventsAndSchema（事件与结构）

EventName（事件名）必须使用稳定 Domain.Action.Result（领域.动作.结果）形式，并在 Schema Registry（结构注册表）中预注册。

第一版预注册 Session.Connect.Succeeded、World.Load.Completed、World.Travel.Completed、Combat.Match.Completed、Commerce.Intent.Created、Commerce.Payment.VerificationResult、Commerce.Fulfillment.Result 及 Development Foundation（开发基础）事件。

Attributes（属性）只支持 String/Int64/Double/Bool（字符串/64位整数/双精度测量/布尔）。禁止任意嵌套 JSON Blob。

UE 端和 Go 后端都执行白名单校验。Shared Schema 位于 `Shared/Contracts/GamePlatform/Schemas/telemetry-batch.v1.schema.json`。