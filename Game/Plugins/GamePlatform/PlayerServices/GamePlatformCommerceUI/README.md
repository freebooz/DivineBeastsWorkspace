# GamePlatformCommerceUI（游戏平台商城界面）

更新日期：2026-09-30；当前能力依据正式工作树实际文件和调用链。此前有关后端、迁移、Outbox及假支付已实现的文字被本页替代。

客户端商城类型、目录/订单缓存、C++页面与ViewModel、购买意图/凭据/对账业务Port；默认领域JSON适配通过GamePlatformOnline。客户端只有服务器状态投影，不决定付款或履约成功。

未发现旧文档所称Backend/gameplatform/commerce、生产仓储/履约或DevelopmentFakePaymentProvider。Backend/internal/modules/commerce/doc.go、生成契约与契约测试不能替代支付、钱包账本或奖励闭环。未进行任何真实支付/退款。

| 证据维度 | 当前状态与边界 |
| --- | --- |
| 源码 | 上述UE切片与Private/Tests源码存在；接口定义不等于生产适配器接通。 |
| 模块编译 | 本次修改后的UE5.8 Editor/Client/Server结果由统一执行账本记录；本页不预先宣称通过。 |
| 自动化运行 | 本次已补生命周期/终态回归源码；UE执行结果尚待统一验证。Task 2源码回归通过仅属静态证据。 |
| 后端联调 | 上述缺失适配/事务/授权链未完成；未以开发替身冒充后端。 |
| Cook/联机 | 本次未执行干净Cook、专服加双客户端、真实角色和资产流程。 |
| 人工验收 | 未执行运行体验/设备/视觉及完整中文注释存量验收。 |

客户端接入采用已配置认证的同一GameInstance `UGamePlatformOnlineClientSubsystem`创建传输，然后显式注入领域服务。四服务不保存Token、创建原始HTTP请求或复制刷新/重试预算；写命令未声明端点幂等前不自动重放。旧URL/Token构造是弃用迁移入口，始终未配置；现存工程没有它的正式C++调用者，外部消费者须改用Online构造。取消只撤销本领域句柄及回调代次，不退出Online或取消其他领域。

配置/目标/模块身份保持插件现有声明，不创建空内容包或伪造资产。真实Widget资产必须由Monolith MCP生成、编译、保存、重载留证；目前只有C++基类。

`Docs/TestingAndEvidence.md`列本次验证入口，`Docs/MigrationAndHandover.md`列兼容影响。其余标记为历史设计说明的文档只记录意图，不能证明相应后端已经实现。


## 2026-10-09 本轮公开合同与整改

LocalPlayer默认跟随Online认证事实装配现有Transport；目录、意向、订单的请求代次独立，账号Reset失效全部本地等待。所有六个命令在状态广播前保存账号/请求与共享Transport快照，广播内Reset不得继续旧交易；ViewModel/订单广播后亦复核账号，订单事件传递完成时的值副本。

创建意向要求正整数Quantity和有效RequestId，返回true只表示受理。金额为Price明确币种的最小单位整数；Provider SDK成功不能直接成功发奖，唯一成功显示条件仍是后端order=fulfilled、payment=confirmed、fulfillment=fulfilled。支付Provider ClientToken是仅本次SDK使用的短期材料，不记录日志/遥测或持久配置。结果未知查询原订单，不能用再次付款或虚构奖励解决。

回归入口：`GamePlatform.Commerce.Client.ResetDuringState`、`BackendAuthority`及`OnlineOwnership`。生产支付Provider、路由和后端经济闭环仍须真实联调。

以上Automation源码已维护，但本轮分工阶段没有执行UE构建/Automation。实际Editor、Client、Server、Cook和联机证据由根整改账本统一记录；静态源码门禁与原生CMake结果不替代这些验收。中文审核覆盖本轮修改的公开字段、命令范围、线程、异步终态、账号/资源所有权及Build责任；未据此宣称全部历史源码已完成中文审核。

### 2026-10-09 实例退出与同步回调补充

LocalPlayer服务在Deinitialize开始即关闭实例作用域并推进InstanceGeneration/AccountGeneration。Configure的Reset通知同步关闭服务后，外层账号配置不得恢复Transport或发请求；公开刷新/命令及迟到回调同样拒绝已关闭实例。Cancel与Begin调用以局部Transport保活，避免通知释放成员后旧栈继续访问已析构对象。Initialize仅建立新实例代次，不将旧Completion当作新账号结果。

新增CloseDuringConfigure回归使用本领域实际Subsystem和只在Tests存在的手控Transport，验证退出后不发请求。UE自动化尚未执行，运行结果由统一UE验证补证。

整数解析策略已有本模块私有原生回归（Debug/Release）；UE JsonValidation用例直接调用实际JSON解析入口，尚待引擎运行。Commerce/LiveOps使用UE5.8 `StoreNumbersAsStrings`保留number原始token，线协议仍是JSON number；数学值必须为整数并处于目标int32/int64范围，金额不可截断或静默丢精度。普通double DOM仅允许绝对值不超过2^53−1的精确整数。Progression沿用十进制字符串int64，不借溢出饱和值冒充成功，保留前导零字符串的既有可解析行为。

本次未发现Shared/Backend中对应运营/商店HTTP字段的正式schema；已用测试允许省略目录集合。缺字段继续沿用该行为，明确提供但不是数组则失败，已知对象/标签元素类型严格检查。reward_schedule仅消费数组长度，奖品元素领域结构仍须真实后端合同补证，不将本地数量当授奖权威。
