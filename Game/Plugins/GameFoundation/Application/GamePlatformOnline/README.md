# GamePlatformOnline（游戏平台在线业务插件）

## 当前交付状态

2026-09-21：公共 C++ 头文件与先行 UE 契约测试已落地，供主工程按准确类型名编写调用方。**这不是完整插件实现，当前没有服务定义、模块构建规则或插件描述，不能链接运行。** 不创建空模块或固定成功服务冒充完成。实现目标仍为唯一 Runtime 模块 `GamePlatformOnline`，复用 `GamePlatformCore` 的 `FGamePlatformResult`，实例服务留在私有 `UGameInstanceSubsystem`。

生产传输尚待解决一个安全前置：锁定 UE5.8 的 Windows HTTP 使用 Curl，`CurlHttp.cpp:183` 开启自动跳转，公开 `IHttpRequest` 没有禁用选项，且跳转状态与头被过滤。密码／刷新令牌在请求正文，事后 URL 检查不能阻止 307／308 泄露。必须先提供经验证的公开禁用跳转能力，或由用户明确批准其他传输方案；不得引用引擎私有头、猜测 SetOption 名称或降低安全要求。

## 主工程按真实名称接入

唯一门面头：`Interfaces/IGamePlatformOnlineService.h`。该头汇总配置、请求、结果、认证、资料和诊断类型，不暴露 HTTP／JSON／原始令牌。

| 主工程节点 | 已声明入口 | 处理方式 |
| --- | --- | --- |
| OnlineValidateConfiguration（配置校验） | `IGamePlatformOnlineService::ValidateConfiguration`、`IGamePlatformOnlineService::Get`、`Configure` | 先校验配置；Get 返回空时失败；Configure 还必须验证真实传输安全能力 |
| OnlineProbeService（服务探测） | `ProbeService` | 检查 `Result.IsSuccess()` 和 `bReady` |
| OnlineLogin（登录） | `Login` | 传入 `FGamePlatformOnlineLoginRequest`，检查认证结果 |
| OnlineReadProfile（读取资料） | `GetCurrentPlayerProfile` | 真实读取后端；不使用虚构默认资料 |
| OnlineReady（在线就绪） | `GetAuthentication`、`GetCachedProfile` | 检查 SignedIn、已设置资料、主体与 GameId 一致；不能只检查探测成功 |
| 跨节点资料更新 | `UpdateCurrentPlayerProfile` | 参数必须提供 DisplayName、ExpectedRevision、IdempotencyKey |
| 跨节点刷新 | `RefreshAuthentication` | 一个上下文 single flight；结果中没有原始令牌 |
| 跨节点退出与重登 | `Logout`，完成后按测试次序再次 `Login` | 检查 Disposition，区分本地退出和后端确认撤销 |
| 节点取消 | `Cancel` | 使用该节点保存的逻辑请求句柄，不能取消其他实例请求 |
| 脱敏诊断 | `GetDiagnostics` | 只读状态与计数，不输出凭据或报文 |

主工程计划的 `foundation.onlineflow@1`、`DA_FoundationOnlineFlow`、既有 LoadProbeDefinition／EnterSandbox 工厂归主工程所有，本插件不定义这些身份，不创建或生成资产。当前没有 UE 生成资产或运行流程的证据。

配置值由组合根显式传入，默认不设置地址或游戏身份。证书校验必须开启。开发 HTTP 需明确开关且仅允许字面回环地址；接口不提供跳过证书校验、不提供任意相对路径或任意 URL 扩展入口。

密码由主工程从受控进程环境或受限短期文件读入 `FGamePlatformOnlineLoginRequest::Credential`，以 `MoveTemp` 传给 Login；本插件不读取环境变量、不约定秘密文件路径、不提供控制台登录命令。调用方也不得记录凭据结构。普通字符串销毁不构成内存安全擦除证明。

## 生命周期与结果约定

所有接口仅游戏线程使用；Get 返回非拥有指针，不能跨实例销毁保存。所有异步入口都要求完成函数，受理后统一进入后续 GT 队列完成一次，包括参数失败、取消和实例退出。回调参数只在回调期间借用；跨节点保存需复制。

默认请求为实例生命周期。页面可提供弱 Owner；世界请求须显式选择 World 生命周期并提供本实例的弱 World。Owner／World 失效使请求取消，完成函数仍收到取消，因此调用者必须弱捕获 UObject 并检查有效性，不能裸捕获已经销毁的节点。认证和资料不会因为世界请求取消而自动清空。

Login 不隐式切账号；已有认证或进行中登录时拒绝第二次登录，明确切换应先 Logout。Logout 固定实例级，调用返回前完成本地清理和旧代次失效，短期撤销与新认证隔离。取消等待或服务不可达不表示远端撤销成功。`FGamePlatformOnlineLogoutResult::Disposition` 是区分结果的必要字段。

Core 结果默认 NotExecuted；所有业务载荷只能在检查 `Result.IsSuccess()` 后使用。资料默认 Revision 为 -1，不允许默认盲写。网络状态和认证状态独立；一次业务请求最多一次自动认证重放，403 不刷新，轮换响应丢失不盲重试。读取接口约定总是访问后端；缓存由独立的同步 GetCachedProfile 查询。

## 验证与后续实现

已先添加 `Private/Tests/GamePlatformOnlinePublicContractTests.cpp`，覆盖安全默认值并以编译期断言锁定基础门面签名。测试尚未执行，不是 red/green 通过证据。其名称为 `GamePlatform.Online.Contract.SafeDefaults`，将由正式主工程编译和运行；不能用其他宿主绕过历史空描述文件。

尚未实现／执行：请求队列和传输、认证状态机、资料适配、私有子系统、真实后端 UE 自动化测试、模块装配、UE/UHT 编译、三目标构建、烘焙、真实后端联调。现有三个空描述文件保持原位不动。

接口对应六项锁定 HTTP 契约，公共结果不携带秘密。全局目录规划及主工程依赖由主代理同步，本任务只写当前插件目录；完整文件清单见 [目录规划说明](Docs/目录规划说明.md)。
