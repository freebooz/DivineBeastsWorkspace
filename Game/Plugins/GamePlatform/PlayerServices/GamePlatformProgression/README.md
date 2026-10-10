# GamePlatformProgression

现行职责与依赖边界见 `Docs/Architecture/游戏端核心要求.md`、`Docs/Architecture/游戏端插件系统P0收敛审计.md` 与 `Game/Plugins/插件开发规范.md`；旧四服务器文档不再作为执行基线。


## 2026-09-30设计审查修订

OnViewChanged/GetViewGeneration覆盖首次、加载、错误、清空、轨道增删与定义变化；既有XP/等级事件保持，派生缓存不成为权威。新增UE事件用例未执行。

本次真实源码/Native/静态检查与未执行UE/后端/Cook边界见Game/Saved/Reviews/task2-repair-report.md；旧历史运行证据不自动覆盖本次修改。


## 2026-10-09 本轮公开合同与整改

LocalPlayer默认随Online认证事实装配领域Transport；客户端仅读取后端XP/等级，不写权威成长。首次/新增/删除/错误/Reset和曲线定义变化通过OnViewChanged通知，消费者重新读取只读ViewModel；兼容XP/等级事件保留，仅表达已有轨道变化。账号与请求代次防止迟到响应，重复终态不影响后续读取。

JSON的`tracks`必须存在且为数组，空数组合法，缺失/错类型InvalidResponse并保留旧快照。Level/MaxLevel/CurveVersion在转int32前检查有限、整数、正值与上限；XP和版本使用int64字符串，不复用浮点表示长整数。曲线版本不兼容时视图显示不兼容，不伪造进度。

回归入口：既有Client状态/首次事件用例与`GamePlatform.Progression.Client.OnlineOwnership`集合负例。

以上Automation源码已维护，但本轮分工阶段没有执行UE构建/Automation。实际Editor、Client、Server、Cook和联机证据由根整改账本统一记录；静态源码门禁与原生CMake结果不替代这些验收。中文审核覆盖本轮修改的公开字段、命令范围、线程、异步终态、账号/资源所有权及Build责任；未据此宣称全部历史源码已完成中文审核。

### 2026-10-09 实例退出与同步回调补充

LocalPlayer服务在Deinitialize开始即关闭实例作用域并推进InstanceGeneration/AccountGeneration。Configure的Reset通知同步关闭服务后，外层账号配置不得恢复Transport或发请求；公开刷新/命令及迟到回调同样拒绝已关闭实例。Cancel与Begin调用以局部Transport保活，避免通知释放成员后旧栈继续访问已析构对象。Initialize仅建立新实例代次，不将旧Completion当作新账号结果。

新增CloseDuringConfigure回归使用本领域实际Subsystem和只在Tests存在的手控Transport，验证退出后不发请求。UE自动化尚未执行，运行结果由统一UE验证补证。

整数解析策略已有本模块私有原生回归（Debug/Release）；UE JsonValidation用例直接调用实际JSON解析入口，尚待引擎运行。Commerce/LiveOps使用UE5.8 `StoreNumbersAsStrings`保留number原始token，线协议仍是JSON number；数学值必须为整数并处于目标int32/int64范围，金额不可截断或静默丢精度。普通double DOM仅允许绝对值不超过2^53−1的精确整数。Progression沿用十进制字符串int64，不借溢出饱和值冒充成功，保留前导零字符串的既有可解析行为。

本次未发现Shared/Backend中对应运营/商店HTTP字段的正式schema；已用测试允许省略目录集合。缺字段继续沿用该行为，明确提供但不是数组则失败，已知对象/标签元素类型严格检查。reward_schedule仅消费数组长度，奖品元素领域结构仍须真实后端合同补证，不将本地数量当授奖权威。
