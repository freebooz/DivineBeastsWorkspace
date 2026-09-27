# SecurityAndCheatPrevention（安全与防作弊）

Shipping（正式发布）防线：
1. .uplugin（插件描述）模块 TargetConfigurationAllowList（目标配置允许列表）不包含 Shipping。
2. Client/Server/Editor Target（客户端/服务器/编辑器目标）在 Shipping 显式 DisablePlugins。
3. 高风险注册代码再使用 #if !UE_BUILD_SHIPPING 条件编译。
4. GamePlatformDebugClient（客户端调试模块）为 ClientOnly（仅客户端）。
5. CanContainContent=false，V1 无 Debug Asset（调试资产）Cook（烘焙）。
6. 没有自定义 Remote Debug RPC（远程调试RPC）。

Sensitive Filter（敏感过滤）拒绝 AccessToken、RefreshToken、Authorization、Cookie、Password、ClientSecret、PrivateKey、Signature、Nonce、PaymentReceipt、Receipt、TransferTicket（访问令牌/刷新令牌/授权/Cookie/密码/客户端密钥/私钥/签名/随机数/支付回执/回执/迁移票据）等字段。

命令参数限制长度并按固定 Enum/0|1（枚举/布尔值）解析，不接受任意 UObject Path、File Path、URL、SQL 或 Shell（对象路径/文件路径/网址/数据库语句/系统命令）。

插件没有 GrantItem、GrantXP、CompleteQuest、UnlockHero、SetPaymentSuccess、SetHealth、Revive 或 Teleport（发物品/经验/完成任务/解锁英雄/改支付/改生命/复活/传送）调试命令。