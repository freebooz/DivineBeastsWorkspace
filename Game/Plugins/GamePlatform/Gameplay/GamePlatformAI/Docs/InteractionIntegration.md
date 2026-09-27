# InteractionIntegration（交互集成）

第一版没有实际“AI开门/采集”业务行为，因此 `GamePlatformAIServer`没有直接依赖 GamePlatformInteraction；这符合“只添加真实需要依赖”的原则。

边界已经验证：AI不得直接修改 Door/Pickup/Harvest，未来若新增 AI Interaction Task，应调用 Interaction 的服务器合法入口，继续执行 Target/Distance/State/Concurrency/Revision 验证并等待 Result。

当前不存在为架构完整而绕过 Interaction 的 `Door.bOpen=true`、直接 Consume 或 Harvest 修改。

Interaction Integration运行状态：不适用（本轮无真实AI交互行为）。
