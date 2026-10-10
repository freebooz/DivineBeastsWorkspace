# GamePlatformLiveOps

现行职责与依赖边界见 `Docs/Architecture/游戏端核心要求.md`、`Docs/Architecture/游戏端插件系统P0收敛审计.md` 与 `Game/Plugins/插件开发规范.md`；旧四服务器文档不再作为执行基线。


## 2026-09-30设计审查修订

OnViewChanged/GetViewGeneration覆盖状态/错误/账号清空；同服务器Revision刷新和活动开始/结束时间边界也通知目录/签到派生视图，无需UI轮询。新增UE用例未执行。

本次真实源码/Native/静态检查与未执行UE/后端/Cook边界见Game/Saved/Reviews/task2-repair-report.md；旧历史运行证据不自动覆盖本次修改。


## 2026-10-09 本轮公开合同与整改

LocalPlayer默认跟随Online认证事实，玩家签到/领奖投影属于当前账号，目录属于公开Published投影。Reset清空玩家及领奖状态、广播OnViewChanged/OnPlayerStateChanged和失效OnClaimChanged，不让旧账号展示遗留。UTC边界Ticker只在有认证账号及有效Transport时注册，Reset/退出即撤销；它只处理时间窗口，业务状态仍由事件更新。

所有UTC均来自后端时间样本，客户端不自行滚动权威签到周期或发奖。ClaimOperationId是原操作的幂等身份，未知结果只查询/对账原操作；静态定义、缓存时间和签入展示不能证明奖品到账。

回归入口：`GamePlatform.LiveOps.Client.SameRevisionView`、`TimeBoundary`及既有账号隔离用例。

以上Automation源码已维护，但本轮分工阶段没有执行UE构建/Automation。实际Editor、Client、Server、Cook和联机证据由根整改账本统一记录；静态源码门禁与原生CMake结果不替代这些验收。中文审核覆盖本轮修改的公开字段、命令范围、线程、异步终态、账号/资源所有权及Build责任；未据此宣称全部历史源码已完成中文审核。

### 2026-10-09 实例退出与同步回调补充

LocalPlayer服务在Deinitialize开始即关闭实例作用域并推进InstanceGeneration/AccountGeneration。Configure的Reset通知同步关闭服务后，外层账号配置不得恢复Transport或发请求；公开刷新/命令及迟到回调同样拒绝已关闭实例。Cancel与Begin调用以局部Transport保活，避免通知释放成员后旧栈继续访问已析构对象。Initialize仅建立新实例代次，不将旧Completion当作新账号结果。

新增CloseDuringConfigure回归使用本领域实际Subsystem和只在Tests存在的手控Transport，验证退出后不发请求。UE自动化尚未执行，运行结果由统一UE验证补证。

整数解析策略已有本模块私有原生回归（Debug/Release）；UE JsonValidation用例直接调用实际JSON解析入口，尚待引擎运行。Commerce/LiveOps使用UE5.8 `StoreNumbersAsStrings`保留number原始token，线协议仍是JSON number；数学值必须为整数并处于目标int32/int64范围，金额不可截断或静默丢精度。普通double DOM仅允许绝对值不超过2^53−1的精确整数。Progression沿用十进制字符串int64，不借溢出饱和值冒充成功，保留前导零字符串的既有可解析行为。

本次未发现Shared/Backend中对应运营/商店HTTP字段的正式schema；已用测试允许省略目录集合。缺字段继续沿用该行为，明确提供但不是数组则失败，已知对象/标签元素类型严格检查。reward_schedule仅消费数组长度，奖品元素领域结构仍须真实后端合同补证，不将本地数量当授奖权威。

目录、玩家状态、领取、领取结果查询分别拥有独立请求代次与在飞门闩。领取回包OperationId须可解析并匹配发起时捕获的Guid，错身份产生InvalidResponse；旧/重复结果不能消费后续领取资格。
