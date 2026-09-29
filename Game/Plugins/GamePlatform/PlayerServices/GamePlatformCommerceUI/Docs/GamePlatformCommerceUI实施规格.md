# GamePlatformCommerceUI 实施规格

版本：1.0.0  
日期：2026-09-29

## 1. 目标与边界

本规格把 `GamePlatformCommerceUI` 从未接通的客户端原型收敛为可验证的交易客户端纵向切片。插件只负责商品目录、购买意图、订单状态、恢复命令和只读界面状态；支付、价格、扣款、履约和发奖权威全部属于 Go 后端。

保持现有插件 `GamePlatformCommerceUI`、模块 `GamePlatformCommerceUIClient` 和 `ClientOnly` 宿主身份，不新增第二套 Commerce 插件，不新增独立 Commerce 服务入口。后端能力装配在现有 Gateway 与 PlayerData 路径中，生产未配置支付提供商时必须失败关闭。

项目视觉资产不属于平台插件。神兽联盟商城页面的 C++ 适配位于 `DBAClient/DivineBeastsUIClient`，Widget Blueprint 位于 `DBAUIPack_Core`，资产只能由 Monolith MCP 创建、编译、保存和复核。

## 2. 唯一契约

新增 `Shared/Contracts/GamePlatform/OpenAPI/commerce.openapi.yaml`。契约至少覆盖：

- 获取当前认证玩家目录；
- 创建购买意图；
- 以购买意图幂等创建订单；
- 查询单个订单；
- 查询当前玩家可恢复订单；
- 提交不透明支付回执；
- 触发订单对账。

玩家身份只能从 Bearer 认证得到。修订号和金额最小单位使用 `int64` 语义并以十进制字符串传输；数量使用有界正整数；错误分支只读取稳定 `errorCode`，不得解析自然语言消息。未知枚举必须失败关闭。

## 3. 后端语义

Commerce 领域位于 `Backend/internal/modules/commerce`，包含领域类型、服务端口、幂等内存实现和测试。PlayerData 仅作为长期玩家业务组合入口，不复制 Commerce 规则；Gateway 只做认证主体提取、严格 JSON 解码和线格式转换。

订单成功必须同时满足：订单为 `fulfilled`、付款为 `confirmed`、履约为 `fulfilled`。Provider 成功、客户端回执或付款确认都不能单独产生奖励成功。

开发假提供商仅允许本地组合根显式启用。生产组合根在没有真实提供商和仓储配置时不注册可购买能力，返回 `PROVIDER_UNAVAILABLE` 或 `SERVICE_UNAVAILABLE`，不得固定成功。

所有写操作使用稳定业务身份：创建意图使用 `requestId`，创建订单使用 `purchaseIntentId`，回执使用 `receiptSubmissionId`。同键同内容返回原结果，同键异内容返回 `IDEMPOTENCY_CONFLICT`。结果未知后客户端必须复用原身份恢复。

## 4. UE 客户端语义

`UGamePlatformCommerceClientSubsystem` 保持 `ULocalPlayerSubsystem` 作用域。账号切换、退出和反初始化必须取消请求、推进代次，并清空目录、购买意图、订单、错误、待处理操作和敏感临时值。旧账号回调不得提交到新账号。

公开只读快照 `FGamePlatformCommerceClientSnapshot` 至少包含状态、错误、快照修订、目录、当前意图、当前订单、待处理操作和命令可用性。任一可观察字段变化都必须广播，不能只在状态枚举变化时通知。

所有 Commerce 网络请求必须通过 `UGamePlatformOnlineClientSubsystem::SendAuthenticatedRequest`。Commerce 不接收、不保存、不返回 Access Token，也不接受任意绝对 URL。具体传输类留在 Private，使用相对路径、统一请求句柄、截止时间、幂等键和 `bMayHaveReachedServer`。

Provider Client Token 是短生命周期敏感值，不进入通用 Blueprint ViewModel、配置、日志、遥测或资产默认值。未选定真实支付 SDK 前只保留不透明 Provider 会话标识和明确的开发环境标志。

## 5. 项目界面

新增 `UDivineBeastsStoreScreen` 作为项目层页面适配，注册 `UI.Screen.Store`。页面只订阅 Commerce 快照并提交命令，不直接发送 HTTP、不持有权威交易状态、不使用 Tick 轮询。

`WBP_DBA_UI_Store` 由 Monolith 创建到 `DBAUIPack_Core`，父类为 `UDivineBeastsStoreScreen`。必须完成编译、保存、重载、Widget 树、焦点、手柄导航、安全区、可访问性和本地化复核，并记录到 `Docs/MonolithGenerationManifest.json`。

## 6. 验收

- Shared 契约检查通过，Commerce operationId 全局唯一；
- Go Commerce 领域、Gateway 和 PlayerData 定向测试通过；
- 账号 A 已加载目录后切换账号 B，目录归零且迟到回调被忽略；
- 金额和修订号不经过 `double`，小数、溢出、负数和重复标识被拒绝；
- 订单结果未知后使用原业务身份恢复；
- UI 仅在后端三状态全部完成后显示成功；
- Commerce 客户端模块通过锁定 UE5.8 Editor/Client 定向构建；
- Server 目标依赖闭包不包含 Commerce 客户端模块或商城视觉资产；
- Monolith 资产编译、保存、重载和清单证据完成；
- 文档只陈述已执行的真实检查，不把静态检查、编译、Cook、联机或人工验收混为一谈。
