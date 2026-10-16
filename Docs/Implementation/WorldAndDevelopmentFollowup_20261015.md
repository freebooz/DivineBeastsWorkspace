# 2026-10-15 世界操作、冰箭、鸟群与开发登录收口

本记录为本轮最终状态；既有制作清单中的“CookPending”和旧执行段落保留当时阶段，最终构建、容器及运行结论以本记录列出的新证据为准。工作区存在用户及其他开发修改，未重置、提交或覆盖其内容。

## 修改与边界

- A/D 默认改为 `DBA.MoveRight` 的 -1/+1 侧移，保留 Q/E 同义侧移；移除默认 A/D 转向绑定。鼠标水平与俯仰系数均为 +1。新客户端显式加载工作区 `DefaultInput.ini`，避免旧生成配置沿用旧绑定。
- 登录真实蓝图经 Monolith 保存与原生重载，包含 45 个控件。新增 player01—player10 快捷按钮与“进入插件化开发模式”入口；快捷按钮复用真实登录命令，开发口令只经瞬态进程环境与命令参数使用。页面更新、忙碌与完成仍通过事件通知；页面失活解绑并清理，拒绝旧代次重入。插件入口使用既有独立门户及完整 Editor 构建门禁。
- 冰箭原位修复 CustomHlsl 的输入分词、ParameterMap 暴露及 Spawn/Update 缓存副作用。六系统的真实 GPU 采样和原生视口均出现可见箭身、碎冰；正式发布增加同一次 Cook 的着色器诊断门禁，避免“资源存在但 GPU 编译失败”再次漏检。
- 鸟群三模式改为 S 型或伸长八字掠行与公共漂移，实际整数模式桥接到原模块，不再使用原圆周路线。保持 240/180/380cm 间距合同、稳定 ID、既定数量和固定 Bounds；旧 Float 影子参数明确登记为遗留，并未伪称已全部删除。
- 保留已完成的目标冻结追踪、零气势法术、冰/火施法条、组队竞技及事件 UI；本轮未重新设计这些机制，也未重建身份或玩家资料容器。

## 本次实际验证

1. 完整 Client/Server：`Saved/Validation/FoundationM0/65fe0732-83c8-4415-aae4-8fe5c1993da7/Build/result.json`，两个目标退出 0。完整 Editor：`9cba8b78-de12-4e2a-8a67-e5df71b00948/Build/result.json`，Passed/Eligible；保持锁定 UE5.8、既有 NoSharedPCH 配置及 NoUBA，未降低版本或关闭模块。
2. 最新独立原生聚合 39 项全部 Success，进程退出 0：`Saved/Validation/WorldEntryFix20261015/FollowupNativeVerified.json`。包含原 33 项，以及十账号配置/真实控件生命周期、插件启动生命周期/无世界失败、登录控件/密码显隐回归。保留两项既有非致命警告，不声明零警告。真实 D3D RHI 同批生成五张新截图，现有颜色发布门禁再次通过；曾出现的路径规范化指纹错误已留存诊断并使用原始门禁函数纠正。
3. 新路线真实 GPU：九配置、两时刻共 18 项通过，原始 GPU 坐标、间距、哈希及运行身份见 `RoutesGPUVerified.json` 与 `Saved/Validation/BirdFlockP4P7/GPUReview/SpacingReview/GPUReview.json`。独立冷重载、78 组合真实网格包络审计及 43 项纯算法回归见鸟群专项记录。
4. 新 Client Cook/Stage：`ea610e38-13b5-451b-af74-92cbe65c500f/Cook-FrontEndClient/result.json`，Passed。实际 IoStore 基础 540 项、施法/冰箭 48 项均无缺失；同次 Cook 的六个冰箭阶段齐全，ShaderDiagnostics/Errors 为空。鸟群容器门禁通过。
5. 新 Server Cook/Stage：`cd4f8a4b-da18-4ef5-9c01-9722caa61937/Cook-Server/result.json`，Passed。逐个读取三容器，禁止表现项为 0，Village/MainArena 地图均存在。四份 FileOpenOrder 原始字节已恢复，新生成内容独立归档。
6. 新运行证据：`Saved/Validation/WorldEntryFix20261015/Runtime.json` 及 `FollowupRuntimeVerified.json`。客户端 PID53732/42860 均有 SSLM 可见窗口、登录页日志且无 Fatal，已启用十账号及插件开发入口。Village PID52348；MainArena 池宿主 PID54732。五个后端 `/health/ready` 均 200，两个服务器 Boot 认证读回通过；空竞技场 assignment 404/`GAME_SERVER_ASSIGNMENT_NOT_FOUND` 是尚未分配状态，不冒称其已开赛。
7. 删除已被替换的 0142957a/ea85c485 两包的四个 Stage/Cooked 目录，释放 1.754GiB，见 `FollowupOutputCleanup.json`。加此前本轮旧包清理约 3.50GiB；源资产、有效脚本、账户数据、日志及共享 Zen 缓存保留。本轮自有独立 Zen 助手已精确停止。

## 未覆盖与已知限制

真实新包的快捷账号点击、门户点击、A/D 手感、联网冰箭、长时间鸟群路线及组队对战/返回仍需人工体验；运行窗口、原生测试和容器清单不能替代人工玩法验收。此前用户确认世界进入成功属于先前运行包。本轮专服仍有既有 CommonUI viewport 与被剥离 InventoryClient 类型配置诊断，未扩展为无关配置重构。

`GPUViewportPiloted.png` 是冰箭真实原生视口可见证据；先前黑色 FPreviewScene 和未正确切换镜头的截图保留为诊断，不能作为通过证据。最后为只读 GPU 探针补充的冷启动 SimCache 就绪等待仅完成语法检查，未重新运行冷启动探针；已通过的资产 GPU/视口证据明确为编译完成后的运行，不将工具改动冒称冷启动实测通过。

中文说明检查范围为上述实际受影响的输入、登录绑定/凭据/生命周期、启动器、冰箭作者/修复/门禁与鸟群作者/校验链路；未全量审计历史源码。前期构建参数切换曾触发引擎缓存重编，失败与中断记录保留；本次以最终成功目标及新 Cook 为准，不掩盖失败，也不把重复检查当作额外完成量。
