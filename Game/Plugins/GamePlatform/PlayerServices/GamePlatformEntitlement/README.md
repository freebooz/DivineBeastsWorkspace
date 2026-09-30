# GamePlatformEntitlement（游戏平台权益）

更新日期：2026-09-30；当前能力依据正式工作树实际文件和调用链。此前有关后端、迁移、Outbox及假支付已实现的文字被本页替代。

共享权益类型与查询；LocalPlayer快照缓存、账号代次隔离；默认领域JSON适配通过GamePlatformOnline受保护通道读取。

未发现旧文档所称Backend/gameplatform/entitlement、0003权益迁移、仓储/Outbox/Quest奖励消费链或正式服务器英雄权益授权适配。Backend/internal/modules/entitlement/doc.go是领域说明，不能充当运行实现。

| 证据维度 | 当前状态与边界 |
| --- | --- |
| 源码 | 上述UE切片与Private/Tests源码存在；接口定义不等于生产适配器接通。 |
| 模块编译 | 本次修改后的UE5.8 Editor/Client/Server结果由统一执行账本记录；本页不预先宣称通过。 |
| 自动化运行 | 本次已补生命周期/终态回归源码；UE执行结果尚待统一验证。Task 2源码回归通过仅属静态证据。 |
| 后端联调 | 上述缺失适配/事务/授权链未完成；未以开发替身冒充后端。 |
| Cook/联机 | 本次未执行干净Cook、专服加双客户端、真实角色和资产流程。 |
| 人工验收 | 未执行运行体验/设备/视觉及完整中文注释存量验收。 |

客户端接入采用已配置认证的同一GameInstance `UGamePlatformOnlineClientSubsystem`创建传输，然后显式注入领域服务。四服务不保存Token、创建原始HTTP请求或复制刷新/重试预算；写命令未声明端点幂等前不自动重放。旧URL/Token构造是弃用迁移入口，始终未配置；现存工程没有它的正式C++调用者，外部消费者须改用Online构造。取消只撤销本领域句柄及回调代次，不退出Online或取消其他领域。

配置/目标/模块身份保持插件现有声明，不创建空内容包或伪造资产。

`Docs/TestingAndEvidence.md`列本次验证入口，`Docs/MigrationAndHandover.md`列兼容影响。其余标记为历史设计说明的文档只记录意图，不能证明相应后端已经实现。
