# GamePlatformOnline 执行进度与断点

日期：2026-09-21。实际工作空间 `E:/poject/feebooz/DivineBeastsWorkspace`，正式工程 `Game/DivineBeastsArena.uproject`。当前记录为进行中，不表示验收通过。

## 授权与现场

最新用户附件明确授权第四插件 Online、公共共享契约、身份/网关/资料三服务、必要迁移及隔离本地联调。保留五个后端应用，不实施 Session、切服、匹配或正式世界。并发工作树中的 GamePlatformSession、postgresadmission 和000003迁移属于其他任务，不覆盖、不混为本次成果。

根AGENTS生效；实际中文插件规范与总体规划继续使用，不新造缺失的OverallPlan.md。FoundationM0独立设计/任务/AcceptanceChecklist缺失的既有记录保留。前三插件及薄主工程已写入；原生回归不等于UE验收。M0最后断点已增量记录在其ExecutionProgress/Verification。

唯一引擎仍为本机UE5.8.0/CL0，源码根由执行时EngineRoot或UE_ROOT取得；用户明确保留三份空历史描述原位。因此本轮不得移动、修补、禁用或新建宿主绕过。没有把正式工程构建失败降为“只是缺引擎”。

后端基线仅guest会话、资料修订写入，无真实账号凭据验证和持久幂等。内部已有gRPC及HTTP适配。本轮采用保留接口的增量密码提供方和PG仓储，详见BackendContractMap。

## 实施顺序及边界

1. 已完成现场只读核对、契约映射初稿及M0断点保全。
2. 正在补齐真实账号/会话原子轮换与撤销、资料幂等PG事务、共享契约与原内部传输。
3. Online唯一运行模块正在实现，私有子系统通过公开类型化服务访问，Core结果复用，无Data/Flow反向依赖。
4. 主工程显式Online入口通过现Flow工厂装配；新增独立在线流程资产生成阶段，不覆盖单机流程定义。
5. 准备隔离后端三服务及PG脚本，实际运行前核对所用容器/网络/端口归属；不执行既有五服务全量部署，不迁移其他数据库。
6. 最后分别记录Go/UE/真实持久化/多实例/图形/烘焙，未执行保持未执行。

## 本轮必要前置修复

主工程 Ready 屏障保存并复核FlowHandle、切图操作GUID及实际世界；取消仅撤销WorldContext中严格匹配自己的尚未消费请求，已消费切图无法回滚但旧结果不得Ready；启动截止检查移至世界等待之前。UE5.8本地GameInstance::GetWorldContext、WorldContext::TravelURL及OpenLevel延期消费源码已核对。

Host原生测试先因新增生产策略缺失失败，补实现后19断言通过（构建/CTest退出0）；它只证明入口策略，不证明UE切图。Data外部DownloadParams保留、已释放租约历史增长与Flow非强持有Payload接纳风险未扩展修复，Online项目Payload由GI强引用保持，仍将一般能力风险交付记录。

资产脚本增加显式`--phase OnlineFlow`，仅创建/校验`DA_FoundationOnlineFlow`（`foundation.onlineflow@1`），保持单机默认图。新增测试先3项缺实现错误，补实现后离线48项通过；未生成任何二进制资产。

## 凭据及退出码约定

### 用户批准的内部Go生成路径修复

核对发现原内部Proto go_package为`generated/proto/internal/...`，现有`internal/transport`导入不满足Go internal包边界。用户已明确批准将Go生成路径改为`internal/generated/...`并机械更新现有适配导入；RPC package、方法、字段号及线上消息不变。该授权仅覆盖生成路径修复与锁定工具重生，不扩大比赛/服务器控制业务，也不改Session并发实现。

密码与令牌不得进入普通配置、命令参数、日志或源码。短期文件只允许当前运行拥有且受限访问的秘密输入，日志仅记录文件路径/RunId和脱敏结果。工具返回0仅所声明检查通过；1有失败；2存在必需未执行。协议脚本成功不等于UE真实联调完成。
