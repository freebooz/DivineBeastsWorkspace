# TestingAndEvidence（测试与证据）

静态验证入口为 `Build/Validation/VerifyCommerce.ps1（商城验证入口）`，依次执行 Backend、PaymentProvider、Fulfillment、Client（后端、支付提供器、履约、客户端）四套门禁。

Go 测试源码覆盖 Catalog（目录）、整数金额、Offer 时间窗口、Unsupported Progression/Free 商品（不支持成长/免费商品）、Intent Price/Reward Snapshot（购买意图价格/奖励快照）、TTL（有效期）、订单状态转换、Fulfillment OperationId（履约操作编号）以及 Fake Provider（假支付提供器）的 production 禁用/success/failure/timeout（生产禁用/成功/失败/超时）。

UE 自动化测试源码覆盖 Catalog load（目录加载）、CreateIntent 双击 Debounce（创建意图防重复点击）、AwaitingProvider（等待支付提供器）、Receipt 后 AwaitingFulfillment（提交凭据后等待履约）、只有后端 Fulfilled（已履约）才 Succeeded（成功），以及账号切换旧回调隔离。

静态门禁不等于 Go test、PostgreSQL Integration（数据库集成）、Receipt Replay 并发（凭据重放并发）、真实支付 SDK、UE Automation、Multi-PIE、Client Build/Cook（UE自动化、多编辑器玩家、客户端构建/烘焙）。没有实际证据时均为未执行。