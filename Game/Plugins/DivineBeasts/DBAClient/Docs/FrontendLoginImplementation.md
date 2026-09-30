# 神兽联盟前台启动与登录闭环实施说明

> 更新日期：2026-09-29
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
        └── GamePlatformOnlineClient（平台认证/受保护请求唯一生产状态机）
                       ↓
              FGamePlatformGatewayAuthProvider（平台私有 Gateway 传输）
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

项目层不再实现认证 Provider，也不持有 AccessToken/RefreshToken。`UDivineBeastsApplicationFlowSubsystem（项目应用流程子系统）` 仅注入 Gateway 地址、GameId 和 ClientVersion；`UGamePlatformOnlineClientSubsystem（平台在线客户端子系统）` 统一实现 `IGamePlatformOnlineService（在线服务门面）`，其私有 `FGamePlatformGatewayAuthProvider（平台 Gateway 认证提供者）` 负责真实 HTTP/JSON、Token 生命周期、401 单次共享刷新、安全重试与同源授权。

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

### 8.1 已交付的 Widget Blueprint / RootLayout 与剩余边界

2026-09-28 已通过 Monolith MCP 0.20.3 在正式工程中创建、编译、保存并在编辑器重启后回读：

~~~text
WBP_DBA_UI_RootLayout
WBP_DBA_UI_Login
~~~

RootLayout 与 Login 均为真实 `.uasset`，其父类、Widget Tree、命名控件、密码掩码和磁盘状态已经回读；两个蓝图编译均为0错误／0警告，登录页可访问性审计为0问题。Boot 与 LoadingTravel 仍是后续页面。只有真实流程 Definition、RootLayout 运行安装、相关页面资产和后端服务共同可用后，才能宣称完整登录运行闭环。

登录页当前采用临时极简视觉：页面和用户名／密码输入框均为黑色背景，输入框使用灰色边框并在聚焦时显示蓝色描边，蓝色“登录”按钮作为唯一主要操作；不使用卡片或面板。忙碌、维护和错误控件默认隐藏，仅由现有事件状态显示，不增加业务轮询。

### 8.2 登录后的项目业务 API

认证、玩家资料、持久角色与世界分配已经统一对齐 `Shared Gateway（共享网关）` 正式契约，唯一真源为：

~~~text
Shared/Contracts/GamePlatform/OpenAPI/gateway.openapi.yaml
~~~

当前正式接口为：

~~~text
GET  /v1/player/profile
GET  /v1/player/characters
POST /v1/player/characters
POST /v1/player/character-selection
POST /v1/divinebeasts/world-entry
~~~

`FDivineBeastsHttpApplicationBackend（神兽联盟 HTTP 应用后端适配器）` 已直接消费上述接口：

- `LoadProfile`：读取玩家资料与 `OnboardingState（新手引导状态）`；
- `LoadRoster`：读取当前账号的持久角色列表；
- `CreateCharacter`：使用 `creationRequestId（创建请求幂等键）` 创建角色；
- `SelectPersistentCharacter`：提交角色编号及期望角色修订号，由后端完成所有权、状态与并发版本校验；
- `RequestWorldAssignment`：只提交已验证角色、目标体验与区域偏好，由后端权威产生 World Assignment（世界分配）、Endpoint（端点）和一次性 TransferTicket（切服票据）。

因此源码层正式闭环已经覆盖：

~~~text
Boot
→ Initialize
→ Authentication / Login
→ LoadProfile
→ LoadRoster
→ CharacterEntry
   ├─ 无角色 → CharacterCreate → CreateCharacter
   └─ 有角色 → CharacterSelect
→ ValidateSelection
→ ResolveExperience
   ├─ 未完成新手引导 → Village.Tutorial
   └─ 已完成新手引导 → OpenWorld.Hub
→ RequestWorld
→ TransferWorld
→ WorldReady
→ InWorld / Playing
~~~

`CharacterEntry（角色入口）` 的创建/选择分流由项目 `ApplicationFlow（应用流程）` 与 `RoutingPolicy（界面路由策略）` 共同投影，不新建第二套 UI 状态机。角色创建成功后流程设置待验证选择并进入 `ValidateSelection（角色选择验证）`；客户端不能绕过后端权威选择结果。

### 8.3 前台三维场景与正式世界边界

当前真实前台地图已经存在：

~~~text
/DBAFrontEndPack/Maps/L_DBA_FrontEnd
/DBAFrontEndPack/Maps/L_DBA_CharacterStudio
~~~

其中 `L_DBA_FrontEnd（前台宿主地图）` 用于 Boot / Login 等客户端前台；`L_DBA_CharacterStudio（角色预览工作室）` 用于 CharacterSelect / CharacterCreate 的三维角色预览。两者只由 `DBAFrontEndPack（神兽联盟前端三维场景内容包）` 持有，只进入 Client / Editor，不属于 OpenWorld、Village 或 MainArena Dedicated Server（专用服务器）正式世界。

当前已通过 Unreal Editor（虚幻编辑器）真实生成 `/DBAWorldPack_Village/Maps/L_Village_Start`，并同步生成 `Village.Main / Village.Tutorial / Village.Training` 三个 `UDivineBeastsWorldDefinition（神兽联盟世界定义）` 资产。该地图当前定位为“流程闭环验证用最小正式地图”，包含出生点、可碰撞验证地面与基础照明，用于 Login → CharacterCreate/Select → Village → WorldReady/InWorld 的工程验收；它不是最终“湖心三岛桃花新手村”美术地图，后续 PCG、地形、建筑、植被与水体内容应继续在 `DBAWorldPack_Village（新手村世界内容包）` 内增量替换和完善。

### 8.4 Application Flow 正式资产

项目流程运行依赖稳定逻辑身份：

~~~text
divinebeasts.application.main@1
~~~

仓库已提供 `Tools/AssetTools/CreateDivineBeastsApplicationFlowAsset.py（神兽联盟应用流程资产生成器）`，它只能通过真实 UE5.8 Editor 反射创建 `/DBAClient/Definitions/DA_DivineBeastsApplicationFlow`，并严格校验 14 个正式节点、具名路由及循环策略；禁止生成伪 `.uasset`。只有该 DataAsset（数据资产）真实落盘并能被 AssetManager（资产管理器）扫描、Data Lease（数据租约）成功取得后，`UDivineBeastsApplicationFlowSubsystem::StartFlow` 才会调用平台唯一状态机。

2026-09-30 发布验证已生成上述真实资产，并在独立 UE5.8 进程重新加载后校验14节点与逻辑身份，退出码0。生成器对 EditDefaultsOnly 节点采用结构体构造初始化，已有资产只校验，不能自动覆盖；存在但加载失败时明确拒绝。本次证据为 `Saved/Validation/LoginVillageRelease/20260930/VerifyApplicationFlowReopen.log`。此结果只证明定义资产保存与重载，角色创建／选择等规划页面和世界就绪事实的生产调用方仍须单独实现与验证，不代表登录到新手村已经闭环。

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
