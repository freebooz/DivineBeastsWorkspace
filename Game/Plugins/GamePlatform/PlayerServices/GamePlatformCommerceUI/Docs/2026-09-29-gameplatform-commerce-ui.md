# GamePlatformCommerceUI Implementation Plan

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 GamePlatformCommerceUI 从客户端原型实现为具有统一契约、后端权威、统一认证传输、账号隔离快照和 DBA 项目页面的可验证纵向切片。

**Architecture:** Shared OpenAPI 是唯一跨语言真源；现有 Gateway/PlayerData 承载 Commerce 调用，Commerce 领域保持独立；UE LocalPlayer 子系统通过 GamePlatformOnline 发起同源认证请求并发布事件快照；DBAClient 只做项目路由和页面适配，视觉资产归 DBAUIPack_Core。

**Tech Stack:** UE5.8 C++、Unreal Automation、Go 1.23+、OpenAPI 3.1、PowerShell/Pester、Monolith MCP。

**Spec:** `Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI/Docs/GamePlatformCommerceUI实施规格.md`

## Global Constraints

- 保持 `GamePlatformCommerceUI` 与 `GamePlatformCommerceUIClient` 稳定身份，不新增 Commerce 插件或独立服务入口。
- 所有人工维护代码具备完整中文职责、边界、参数、错误、并发与生命周期说明。
- 不覆盖工作树中与 Commerce 无关的未提交修改；共享文件只做最小增量。
- UI 资产只能通过 Monolith MCP 修改，禁止文件工具伪造 `.uasset`、`.umap`。
- 客户端永远不持有 Access Token，不以 Provider 成功代替后端付款和履约确认。
- 生产没有真实 Provider 时失败关闭；测试假实现不得进入生产组合。
- 本工作树在 `main` 且存在用户未提交修改；不自动提交、推送、合并或发布，以任务账本和定向 diff 代替每任务提交点。

## Review Focus

- 账号 A 已有目录后切换账号 B：目录、意图、订单、错误和敏感临时值必须全部清空。
- 创建订单已到达服务器但响应丢失：重试必须复用 `purchaseIntentId`，不得创建第二笔订单。
- 超过 IEEE-754 精确范围的金额/修订号和小数数量：必须拒绝，不能截断。
- 同状态下错误码变化：快照修订和通知必须变化，UI 不能永远停留在旧错误。
- 生产未配置 Provider：必须返回稳定不可用错误，不能自动启用开发假支付。

---

### Task 1: Commerce 唯一契约与验证入口

**Files:**
- Create: `Shared/Contracts/GamePlatform/OpenAPI/commerce.openapi.yaml`
- Create: `Backend/tests/commerce_contract_test.go`
- Create: `Build/Validation/VerifyCommerce.ps1`
- Modify: `Shared/Docs/Codegen.md`

**Interfaces:**
- Produces: `/v1/commerce/*` 路径、稳定 DTO 字段、错误码和 operationId。
- Consumes: 既有 Bearer 认证与 `ApiError` 约定。

- [ ] **Step 1: Write the failing contract test**：断言 Commerce 契约存在、金额/修订为十进制字符串、玩家身份不在请求体、operationId 全局唯一。
- [ ] **Step 2: Run RED**：`go test ./tests -run 'TestCommerceContract' -count=1`；Expected: 因契约缺失失败。
- [ ] **Step 3: Implement the OpenAPI contract and validation script**。
- [ ] **Step 4: Run GREEN**：同一 Go 命令与 `pwsh -NoProfile -File ../Build/Validation/VerifyCommerce.ps1 -StaticOnly`；Expected: exit 0。

### Task 2: Commerce 后端领域、幂等恢复与 Gateway

**Files:**
- Create: `Backend/internal/modules/commerce/commerce.go`
- Create: `Backend/internal/modules/commerce/commerce_test.go`
- Create: `Backend/internal/modules/playerdata/commerce_bridge.go`
- Create: `Backend/internal/app/gateway/commerce.go`
- Create: `Backend/internal/app/gateway/commerce_test.go`
- Modify: `Backend/internal/modules/playerdata/service.go`
- Modify: `Backend/internal/app/gateway/api.go`
- Modify: `Backend/internal/app/composition/composition_local.go`

**Interfaces:**
- Consumes: Task 1 的 DTO 语义和稳定错误码。
- Produces: `commerce.Service`、`playerdata.Service` Commerce bridge、认证 Gateway 路由。

- [ ] **Step 1: Write failing domain tests**：覆盖同键同内容、同键异内容、价格重算、结果未知恢复、三状态成功门禁和生产 Provider 缺失失败关闭。
- [ ] **Step 2: Run domain RED**：`go test ./internal/modules/commerce -count=1`；Expected: 因类型/实现缺失失败。
- [ ] **Step 3: Implement minimal domain service and development memory repository/provider**。
- [ ] **Step 4: Run domain GREEN**：同一命令；Expected: PASS。
- [ ] **Step 5: Write failing Gateway tests**：覆盖认证玩家、严格 JSON、十进制字符串、可恢复订单和幂等头/业务身份。
- [ ] **Step 6: Run Gateway RED**：`go test ./internal/app/gateway -run 'TestCommerce' -count=1`；Expected: 路由缺失而失败。
- [ ] **Step 7: Implement PlayerData bridge, Gateway handlers and local-only composition**。
- [ ] **Step 8: Run scoped GREEN**：`go test ./internal/modules/commerce ./internal/modules/playerdata ./internal/app/gateway -count=1`；Expected: Commerce 相关包通过；若并行用户改动造成既有非 Commerce 用例失败，账本逐项记录。

### Task 3: UE 账号隔离、快照与订单状态

**Files:**
- Modify: `Source/GamePlatformCommerceUIClient/Public/Types/GamePlatformCommerceUITypes.h`
- Modify: `Source/GamePlatformCommerceUIClient/Public/Services/GamePlatformCommerceClientSubsystem.h`
- Modify: `Source/GamePlatformCommerceUIClient/Private/Services/GamePlatformCommerceClientSubsystem.cpp`
- Modify: `Source/GamePlatformCommerceUIClient/Public/ViewModels/GamePlatformCommerceViewModel.h`
- Modify: `Source/GamePlatformCommerceUIClient/Private/ViewModels/GamePlatformCommerceViewModel.cpp`
- Modify: `Source/GamePlatformCommerceUIClient/Private/Tests/GamePlatformCommerceClientTests.cpp`

**Interfaces:**
- Consumes: Task 1 的状态、错误和恢复语义。
- Produces: `FGamePlatformCommerceClientSnapshot`、快照事件和 Blueprint 命令。

- [ ] **Step 1: Write failing UE tests**：覆盖已加载目录后的账号切换、同状态错误变化、Typed order state、命令可用性和后端三状态成功门禁。
- [ ] **Step 2: Build/run RED**：锁定 UE5.8 构建模块并运行 `GamePlatform.Commerce.Client`；Expected: 新断言失败或缺少新接口导致编译失败。
- [ ] **Step 3: Implement snapshot, full reset, typed states and event revisions**。
- [ ] **Step 4: Build/run GREEN**：同一构建与 Automation 过滤；Expected: exit 0、0 failed。

### Task 4: GamePlatformOnline 安全传输与严格解析

**Files:**
- Modify: `GamePlatformCommerceUI.uplugin`
- Modify: `Source/GamePlatformCommerceUIClient/GamePlatformCommerceUIClient.Build.cs`
- Delete after replacement: `Source/GamePlatformCommerceUIClient/Public/Transport/GamePlatformCommerceGatewayHttpTransport.h`
- Create: `Source/GamePlatformCommerceUIClient/Private/Transport/GamePlatformCommerceGatewayHttpTransport.h`
- Modify: `Source/GamePlatformCommerceUIClient/Private/Transport/GamePlatformCommerceGatewayHttpTransport.cpp`
- Modify: `Source/GamePlatformCommerceUIClient/Private/Tests/GamePlatformCommerceClientTests.cpp`

**Interfaces:**
- Consumes: `UGamePlatformOnlineClientSubsystem::SendAuthenticatedRequest` 和 Task 1 线格式。
- Produces: 不暴露 Token/绝对 URL 的私有 Commerce transport。

- [ ] **Step 1: Write failing parser/transport tests**：覆盖十进制 int64、小数/溢出拒绝、重复标识、稳定 errorCode、幂等键和 OutcomeUnknown。
- [ ] **Step 2: Run RED**：Commerce 模块构建与 Automation；Expected: 旧 double/正文子串实现使测试失败。
- [ ] **Step 3: Replace raw HTTP/token transport with Online-backed private transport**。
- [ ] **Step 4: Run GREEN**：Editor 与 Client 定向模块构建、Commerce Automation；Expected: exit 0、0 failed。

### Task 5: DBA 项目商城页面与 Monolith 资产

**Files:**
- Modify: `Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin`
- Modify: `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/DivineBeastsUIClient.Build.cs`
- Create: `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/Store/DivineBeastsStoreScreen.h`
- Create: `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Screens/Store/DivineBeastsStoreScreen.cpp`
- Modify: `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Screens/DivineBeastsUIScreenCatalog.cpp`
- Modify via Monolith: `/DBAUIPack_Core/UI/Screens/Store/WBP_DBA_UI_Store`
- Modify: `Game/Plugins/DivineBeasts/ContentPacks/DBAUIPack_Core/Docs/MonolithGenerationManifest.json`

**Interfaces:**
- Consumes: Task 3 快照和命令。
- Produces: `UI.Screen.Store` 路由和项目视觉页面。

- [ ] **Step 1: Write failing DBA UI test**：断言 Store 路由解析到项目屏幕且页面不使用 Tick/HTTP。
- [ ] **Step 2: Run RED**：构建 `DivineBeastsUIClient` 并运行 DBA UI Automation；Expected: Store 类型或路由缺失。
- [ ] **Step 3: Implement C++ project screen and route**。
- [ ] **Step 4: Run C++ GREEN**：Editor/Client 定向模块构建和 DBA UI Automation；Expected: exit 0、0 failed。
- [ ] **Step 5: Verify editor identity and create Widget with Monolith**：核对 `Game/DivineBeastsArena.uproject`、Monolith 状态和 UI 动作目录，创建、编译、保存、重载并审计焦点/导航/可访问性。
- [ ] **Step 6: Update Monolith manifest from real tool results**。

### Task 6: 文档、目录登记和最终门禁

**Files:**
- Modify: `README.md` and relevant `Docs/*.md` under the Commerce plugin
- Modify: `Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md`
- Modify: `Docs/Architecture/游戏端插件清单设计.md`
- Modify: `Docs/Architecture/游戏端插件系统P0收敛审计.md`
- Modify: `Backend/DirectoryTree_CN_V1.1.0.md`
- Modify: `Backend/migrations/README.md` only if a migration is actually added

**Interfaces:**
- Consumes: Tasks 1-5 的实际实现和验证日志。
- Produces: 与仓库事实一致的状态、目录和限制说明。

- [ ] **Step 1: Run static and architecture gates**：Commerce 静态门禁、设计基线、插件模块图、继承边界和头文件审计。
- [ ] **Step 2: Run backend verification**：Commerce 定向测试、`go vet ./...` 和可行时 `go test -race`；记录基线并行失败，不掩盖。
- [ ] **Step 3: Run UE verification**：Editor/Client/Server 目标、Automation、Server 依赖剥离和可行的 Cook/Stage；逐项记录真实结果。
- [ ] **Step 4: Update all documents with actual evidence only**。
- [ ] **Step 5: Run `git diff --check`, inspect scoped status/diff and perform final review**。
