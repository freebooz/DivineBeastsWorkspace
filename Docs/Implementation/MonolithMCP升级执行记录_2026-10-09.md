# Monolith MCP v0.23.0 全局与项目升级执行记录

2026-10-09 用户明确要求将 Monolith MCP v0.23.0 作为全局插件，并供所有项目共用。本文只记录本次实际执行结果、验证证据和适用边界；不把文档结论替代引擎加载、MCP 实际调用、游戏运行、Cook 或人工视觉验收。

## 安装范围与复用方式

- 全局引擎级安装位置为 `D:/UnrealEngine-5.8.0-release/Engine/Plugins/Marketplace/Monolith`。
- Codex 全局配置 `C:/Users/Administrator/.codex/config.toml` 中的 `mcp_servers.monolith` 指向上述安装的 `Binaries/monolith_proxy.exe`。配置工作目录 `E:/work/Game/PCG` 实际存在，本次无需修改个人配置。
- 官方描述文件保持 `EnabledByDefault=true`。因此，使用这套 `D:/UnrealEngine-5.8.0-release` 引擎的项目共用同一份引擎级插件，不在各项目内复制 Monolith 源码或二进制。
- 正式项目 `Game/DivineBeastsArena.uproject` 仍显式启用 Monolith，并通过 `TargetAllowList=["Editor"]` 限定为编辑器目标。
- “全局和所有项目”只覆盖使用当前这套 UE5.8 引擎的项目。其他引擎目录、不同 UE 小版本或不同 BuildId 必须分别编译和安装，不能直接复用本次 DLL。

## 官方来源与下载校验

- 官方来源：`tumourlove/monolith` 的 `v0.23.0` 发布。
- 下载文件：`Monolith-v0.23.0-UE5.8.zip`。
- 实际 SHA-256：`550be7b4e5f232bfca0f5800df7e917836fb405b53af02664f1b359405191d89`，与官方发布标记一致。
- 官方包包含 20 个模块、731 个源码文件及全局代理、离线查询工具。只使用发布包内容，不把后续主分支提交冒充 v0.23.0。

## 锁定引擎兼容与重新编译

当前源码引擎为 UE5.8.0，`D:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor.modules` 的真实 BuildId 为 `49444017-86c4-46bd-a6ce-36b57136a1f9`。官方发布包的预编译 BuildId 为 `55116800`，两者不兼容，不能直接复制官方 DLL 或手改模块索引冒充兼容。

本次使用当前引擎的 `RunUAT.bat BuildPlugin`，仅构建 Win64 编辑器宿主，不构建 UnrealGame、服务器、其他平台或 Shipping 产物。实际构建共执行 185 个动作，UnrealBuildTool 退出码为 0，总执行时间 171.38 秒，20 个 Monolith DLL 全部成功链接。官方 `MonolithMaterialActions.cpp` 在 UE5.8 下产生 1 条 C4996 弃用警告，当前仍成功编译；未修改第三方源码以隐藏该警告。

最终安装候选保留官方完整目录、`monolith_proxy.exe` 和 `monolith_query.exe`，仅用本机编译的 20 个 DLL 与 `UnrealEditor.modules` 替换发布包内的异 BuildId 二进制。候选包含 853 个文件、54,265,710 字节，不包含 `Intermediate` 或约 1.45GB 的 PDB 调试中间产物。20 个模块索引与 DLL 数量一致，BuildId 与锁定引擎完全一致；731 个源码文件与官方发布源码逐文件 SHA-256 一致。

## 备份、切换与回退

- 旧版实际为 v0.20.3，不是历史记录中误写的 v0.22.0。
- 工作空间备份位于 `Saved/ToolUpdates/Monolith-0.23.0/Backup/Monolith-0.20.3`，共 1904 个文件、486,363,496 字节。排除可重建的 `Saved` 和 `Intermediate` 后，815 个静态插件文件已逐项 SHA-256 一致。
- 旧版运行索引 `Saved/ProjectIndex.db` 在备份校验时被既有进程占用，无法用普通只读句柄计算源文件哈希；复制件的长度和时间戳一致。该数据库是可重建项目索引，不作为插件源码和二进制兼容证据。
- 切换时先在引擎插件目录创建并验证 v0.23.0 临时候选，再把旧安装移动到 `D:/UnrealEngine-5.8.0-release/Engine/Saved/MonolithUpgradeRollback/Monolith-0.20.3-20261009`，最后将新版移动到固定全局路径。切换命令退出码为 0；若中途失败，命令会把旧目录恢复到固定路径。
- 安装后 853 个文件与最终候选逐文件 SHA-256 一致。全局配置继续使用固定路径，无需因版本升级改变项目或 MCP 地址。

## 真实运行验证

验证时发现已有编辑器进程正在运行正式项目，命令行为 `Game/DivineBeastsArena.uproject /DBAFrontEndPack/Maps/L_DBA_CharacterStudio`。该进程不是本次升级任务启动的，因此本次未关闭、重启或接管该会话，只进行了只读核验。

编辑器日志 `Saved/Validation/VillageFlow/20261009/PreviewRepair.Editor.log` 记录：

- 从固定全局路径加载 `UnrealEditor-MonolithCore.dll`；
- `Monolith 0.23.0 — Core module initializing`；
- MCP 服务在 9316 端口首次尝试即监听成功；
- 项目索引完成，249 个资产和深度索引均为 0 错误。

通过安装后的全局 `monolith_proxy.exe` 进行实际 MCP JSON-RPC 调用，结果为：

- 代理版本：1.1.1；
- Monolith 版本：0.23.0；
- `server_running=true`，端口 9316；
- 项目名称：`DivineBeastsArena`；
- 运行时动作数：1355；命名空间数：26；
- 新增 `input` 命名空间可发现 13 个动作；
- 新增 `localization` 命名空间可发现 4 个动作。

首次代理查询在受限沙箱中被本地套接字策略拒绝，表现为“编辑器不可用”；编辑器日志同时已经证明服务启动。使用获准的本机回环网络权限重跑同一只读查询后全部成功，因此该次失败属于执行沙箱限制，不是 Monolith 模块或编辑器服务故障。

## 验收边界与已知事项

- 已验证：官方包身份、锁定引擎编译、模块 BuildId、全局安装文件、正式项目真实加载、MCP 状态、动作总数以及 v0.23.0 新命名空间。
- 本次没有修改 Monolith 第三方源码，也没有修改项目业务代码或用户界面资产。
- 当前编辑器日志另有角色预览骨骼缺失提示和前端资源加载提示，这些属于既有项目内容问题，不是 Monolith 升级失败，未在本次工具升级中扩大处理范围。
- 本次未验证其他 UE 引擎目录、UnrealGame/Server/Shipping 构建、Cook、设备运行或所有项目逐一启动；这些结果不得从一次编辑器加载外推。
- 当前既有编辑器会话已经加载 v0.23.0。其他此前已启动且加载旧 DLL 的编辑器进程需要在保存工作后自行重启，才能使用新版模块；不得强行终止未确认所有权的编辑器进程。
