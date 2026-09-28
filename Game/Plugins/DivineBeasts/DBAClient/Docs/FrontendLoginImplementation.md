# 神兽联盟前台启动与登录闭环实施说明

> 更新日期：2026-09-28
> 范围：Boot（启动封面）→ Frontend（前台初始化）→ Authentication（认证）→ Login（登录页面）
> 原则：复用现有平台插件，不新建重复代码插件，不伪造 UE 二进制资产。

## 1. 当前正式调用链

~~~text
DivineBeastsArenaClient Target（客户端构建目标）
        ↓
DBAClient（神兽联盟客户端组合插件）
        ↓
UDivineBeastsApplicationFlowSubsystem（项目应用流程）
        │
        ├── GamePlatformApplicationFlow（唯一流程状态机）
        ├── GamePlatformOnlineClient（平台认证状态机）
        └── FDivineBeastsGatewayAuthProvider（项目 Gateway 认证适配）
                       ↓
              GatewayService（统一接入服务）
              POST /v1/auth/login
              POST /v1/auth/refresh
              POST /v1/auth/logout

ApplicationFlow ViewState（流程只读状态）
        ↓
UDivineBeastsApplicationUIAdapter（项目UI适配器）
        ↓
FDivineBeastsUIViewState（UI只读投影）
        ↓
UDivineBeastsUIClientSubsystem（LocalPlayer UI协调）
        ↓
RoutingPolicy（项目主页面路由）
        ├── Boot / Initialize → UI.Screen.Boot
        ├── Authentication    → UI.Screen.Login
        └── Transfer/Ready    → UI.Screen.LoadingTravel
        ↓
GamePlatformUI（CommonUI平台界面）
~~~

UI Widget 不直接发送 HTTP，也不直接推进 ApplicationFlow。

## 2. Boot 启动页

新增 UDivineBeastsBootScreen 和 UDivineBeastsBootViewModel。

Boot 继承项目 Loading Screen（加载页面）基类，因为启动过程属于不可随意 Back 结束的真实初始化事务。

BootViewModel 复用平台 LoadingScreenService（加载界面服务）：
- 真实进度可量化时显示真实进度；
- 无权威进度时保持 Progress < 0；
- 禁止用 Timer/Tick 伪造 0→100；
- 流程进入 Authentication 后，由路由自动切换 Login。

## 3. UI 与 ApplicationFlow 单向适配

新增私有适配器：

~~~text
DivineBeastsUIClient
└── Private/Adapters/Application/
    └── UDivineBeastsApplicationUIAdapter
~~~

它实现既有 IDivineBeastsUIQuerySource 和 IDivineBeastsUICommandPort。

依赖方向：

~~~text
DivineBeastsUIClient
        ↓ Private dependency
DivineBeastsApplicationFlowClient
~~~

ApplicationFlow 不依赖 UI，避免循环依赖。

适配器负责：
- Flow ViewState → UI ViewState；
- Login / TryAutoLogin / CreateCharacter / SelectCharacter / RequestWorld / Logout 命令转发；
- Revision（修订号）和 FlowRun（流程运行）隔离；
- 拒绝旧页面迟到命令；
- 竞技命令不在公共 DBAClient 中伪接线。

## 4. 真实登录认证

项目层新增 FDivineBeastsGatewayAuthProvider，实现平台 IGamePlatformOnlineAuthProvider。

正式契约来源：

~~~text
Shared/Contracts/GamePlatform/OpenAPI/gateway.openapi.yaml
~~~

使用现有接口：

~~~text
POST /v1/auth/login
POST /v1/auth/refresh
POST /v1/auth/logout
~~~

### 开发环境配置

~~~text
DIVINEBEASTS_GATEWAY_BASE_URL=https://your-gateway.example
DIVINEBEASTS_CLIENT_VERSION=0.1.0
~~~

非 Shipping（非正式发布）构建额外允许字面回环地址：

~~~text
http://127.0.0.1:<port>
http://[::1]:<port>
~~~

不允许任意明文 HTTP 主机。

### 凭据安全边界

- Password（密码）只在 UI Command → Flow → Provider 当前调用链瞬时传递；
- AccessToken / RefreshToken 仅保存在 Provider 私有内存；
- Token 不进入 UObject Property、ViewState、日志、遥测、Config、SaveGame；
- RefreshToken 为轮换凭据，刷新失败/超时/响应不确定后不会重放旧令牌；
- Logout 先清除本地令牌，再尽力通知服务端；
- 当前未实现安全凭据持久化，因此 TryAutoLogin 明确返回 AuthExpired，而不是把 Token 写入普通磁盘文件。

## 5. 主页面自动路由

UDivineBeastsUIClientSubsystem 监听项目 ViewState，并在状态变化时同步主页面，不使用 Tick。

~~~text
无 RootLayout
→ 保存状态
→ 不伪造页面已打开

RootLayout 安装成功
→ 立即消费最新状态
→ 创建对应 ViewModel
→ OpenScreenAsync

流程状态变化
→ 取消旧的未完成页面请求
→ 关闭旧主页面
→ 异步打开新主页面
~~~

平台页面打开成功/失败由 GamePlatformUI 事件回传。

## 6. 项目 UI 资源挂载点

已停止使用不存在的 /DivineBeastsUI/ 路径。

规划稳定挂载点：

~~~text
/DBAUIPack_Core/UI/...
~~~

例如：

~~~text
/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Boot
/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Login
/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_LoadingTravel
~~~

`DBAUIPack_Core` 已作为第三层纯内容插件登记。公共用户界面资产必须由 Monolith MCP 在正式 Unreal Editor 工程中创建、编译、保存和回读，禁止以文本占位或改扩展名冒充资产。

## 7. 品牌资源

公司品牌：Freebooz Studio。

游戏品牌：神兽联盟 / Divine Beasts Arena。

源图交接见：

~~~text
Docs/References/Art/Branding/
~~~

规划运行时资产：

~~~text
T_DBA_Brand_FreeboozStudio_Logo
T_DBA_Brand_GameLogo
~~~

二进制 PNG 必须通过可信文件来源进入 Runner，再由 Unreal Editor 正式导入。

## 8. 当前未完成边界

### 8.1 真实 Widget Blueprint / RootLayout

本轮 Monolith 实施范围：

~~~text
WBP_DBA_UI_RootLayout
WBP_DBA_UI_Login
~~~

RootLayout 与 Login 必须由 Monolith MCP 创建并通过编译、保存、回读；Boot 与 LoadingTravel 仍是后续页面。只有真实流程 Definition、RootLayout 安装、相关页面资产和后端服务共同可用后，才能宣称完整登录运行闭环。

### 8.2 登录后的项目业务 API

认证链已经对齐 Shared Gateway 契约。

本轮进一步确认共享契约已经正式提供：

~~~text
GET /v1/player/profile
~~~

因此 FDivineBeastsHttpApplicationBackend::LoadProfile 已改为直接消费
Shared/Contracts/GamePlatform/OpenAPI/gateway.openapi.yaml 中的
PlayerProfile，映射 playerId / revision / tutorialCompleted / defaultWorldId，
不再访问历史 /v1/divinebeasts/profile。

当前仍没有正式共享契约支撑以下项目私有路径：

~~~text
/v1/divinebeasts/characters
/v1/divinebeasts/characters/select
/v1/divinebeasts/world-entry
~~~

因此当前可真实闭环到：

~~~text
Boot
→ Initialize
→ Authentication
→ Login
→ LoadProfile
~~~

LoadRoster → CharacterEntry → OpenWorld 仍必须先在 Shared 契约层正式设计并实现
角色列表、角色选择和世界分配 API，禁止客户端继续依赖未定义的历史路径冒充完成。

## 9. 验收顺序

1. C++ / UHT / Client Module 构建；
2. 核对 DBAUIPack_Core 内容插件登记与挂载；
3. 通过 Monolith 创建并回读 RootLayout / Login Widget Blueprint；
4. 按需导入经授权品牌源图，并继续创建 Boot / Loading Widget Blueprint；
5. 配置 Gateway 环境变量；
6. 启动 Gateway + Identity；
7. 验证错误密码、正确密码、网络断开、维护、退出登录；
8. UE Automation；
9. Cook / Stage；
10. PC 与移动端人工视觉签审。
