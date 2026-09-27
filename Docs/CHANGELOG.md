# 变更记录

保留已有工程变更记录；不根据历史聊天补造不存在的提交或验收记录。

## 2026-09-27｜应用流程与会话准入纵向修复设计（待审核）

- 新增 `Docs/Architecture/游戏流程与会话准入后端纵向修复设计规格.md`，依据当前代码记录 Flow API 断层、GameServerControl 内部路由鉴权缺口、Gateway 玩家分配入口缺失、进程内 Assignment 状态及未接入的 PostgreSQL 准入内核。
- 设计提出 Shared 真源、Gateway 认证主体、受保护控制面、PostgreSQL 准入与 UE 真实连接绑定的一条纵向路径，并将锁定 UE5.8 握手验证设为 Ready/端到端实现门禁。
- 仅新增待审核设计文档并更新索引；没有修改业务源码、契约、数据库迁移或部署，没有运行测试/构建。

## 2026-09-27｜工程缺项修复与真实验证

- 补齐六项真实UE默认配置，并将自动备份归入新增的EditorPerProjectUserSettings默认层；保留原有Engine／Game设置及用户Saved配置。必选配置8/8、总配置9份和实际结构审计通过，三角色、40个GamePlatform身份和46+N边界不变。
- 新增真实工程配置回归、自有头文件预检及失败／正常夹具；Architecture测试60/60通过。预检覆盖主工程／项目插件、续行／注释／字面量和空扫描，实际仍报告234处引用中的4处缺失；不能把检查器自身通过写成项目源码通过。
- 修正DBAWorlds自动化测试残留的独立Lobby合法角色，补全三角色七体验正向映射、旧角色拒绝和合法大厅携带竞技模式拒绝用例；新增与Shared真源一致性回归。UE自动化尚未执行，未改生产角色或生成契约。
- 找到D盘UE5.8源码工具链，原生Session／Loading／ApplicationFlow三个Debug测试入口通过；既有打包配置3项UBT行为测试通过。正式Editor UHT完成，随后完整编译因提交内存压力和120秒超时失败，Client／Server未开始，无Cook或游戏发布。
- 项目旧流程接口和Session真实连接／准入仍未修复；明确权威绑定与取消迁移顺序，不添加空兼容类型。详见[工程缺项修复执行记录](Architecture/工程缺项修复执行记录.md)。
- 独立复核指出的配置层与预检问题已补失败回归并修正；新TestProjectEditorConfig通过UE5.8 UBT验证两个层级和5项设置，不代表编辑器交互或完整UE构建通过。

## 2026-09-27｜三层目录与可选竞技迁移

- 按用户批准方案统一GamePlatform／MobaCommon／DivineBeasts：39个平台插件与MOBA层GamePlatformArena合计保留40个GamePlatform身份；MobaPresentation保持独立两模块。
- GamePlatformArena从GamePlatform/GameModes迁入MobaCommon；新增DBAArena承接原有项目竞技Runtime／Client／Server三个模块。公共项目插件去除竞技硬依赖，竞技客户端对公共流程扩展保留单向依赖，服务器不带入DBAClient。
- 基线更新为46个代码／机制插件＋实际登记内容N；新增ContentPackRegistry及完整中文内容归属规划，当前N=0，无空内容插件和假UE资产。90个迁移文件在移动时逐项哈希一致。
- 统一10个插件描述的旧编辑器层名，修订DeveloperTools层级映射和对应引擎用例；未改变稳定模块、反射或协议身份。新增按目标装配审计与失败夹具，同步所有正式规划、根规则、规范、入口和迁移说明，旧计划保留为历史记录。
- 最终PowerShell架构回归47/47；六种公共／竞技声明装配通过。实际结构审计仍有6个既有默认配置缺失，项目流程仍有旧API引用；UE构建前置返回NotExecuted／2，未编译、Cook、联机或发布游戏。详见[三层架构实施规划](Architecture/游戏端插件三层架构实施规划.md)。

## 2026-09-27｜业务后端核心要求基线

- 新增 `Docs/Backend/业务后端核心要求.md`，统一 Go 业务控制面的领域模块化、五薄入口、跨游戏复用、UE Dedicated Server 权威边界、共享契约、数据一致性、安全、可观测与真实验收要求。
- 明确当前单团队优先采用单 Go Module（Go模块）+ 清晰领域边界，只有在独立扩缩容、故障隔离、数据所有权或发布边界明确时才增加新服务，避免无意义微服务膨胀。
- `Docs/Backend/业务服务说明.md` 增加核心基线入口；同步维护工作空间文档索引与总体目录规划说明。
- `Backend/DirectoryTree_CN_V1.1.0.md` 增加业务后端核心要求入口，确保从后端源码目录开展开发时也能直接定位现行核心基线。

## 2026-09-27｜恢复三角色并将大厅归入 OpenWorld

- 按用户最新明确要求覆盖先前四角色中间方案：正式服务端角色为 OpenWorld、Village、MainArena；大厅使用 OpenWorld 角色，`Experience.OpenWorld.Hub` 为 OpenWorld Profile 默认体验。
- `Experience.Lobby.Main` 仅作为历史兼容体验标识继续映射到 OpenWorld；不再创建或注册 `GameServer.Role.Lobby`，不保留独立 Lobby Profile。因移除已发布角色，Shared 契约提升为 2.0.0，当前兼容范围为2.x。
- 更新 Shared 真源、Go/C++ 生成物、后端注册与分配、Agones 标签、新玩家默认落点、部署 Profile、UE 角色过滤、架构规格和目录树。Go全量测试、vet、race及生成器`-check`通过；角色/Profile Pester 8/8。整体Architecture Pester尚有1项旧DBAClient模块依赖失败；真实工程结构审计为45/45插件、3/3 Target、2/8默认配置，缺少6项配置；UE构建未运行。

## 2026-09-27｜游戏端核心要求基线

- 新增 `Docs/Architecture/游戏端核心要求.md`，统一客户端与 Dedicated Server 的插件化、多项目复用、独立解耦、边界定义、独立演示、人工审核、端侧权威、三服务器角色、1v1～5v5、GAS、数据驱动和真实验收核心要求。
- 明确当前单团队开发采用“按职责/复用/端侧/生命周期/测试边界适度拆分”，禁止机械拆分空插件。
- 修正文档入口及总体目录规划中仍存在的“当前四角色”表述为现行三角色；历史变更记录中的旧阶段事实保留，不回写伪造历史。

## 2026-09-26｜设计基线整合实施（任务 1–2）

- 新增只读结构审计与 Pester 回归测试，7/7 通过。当前实际结构为37/44描述、30/40平台插件、0/4 DBA、1个工程、3个Target、2/8默认配置；审计明确报告57项差异。
- Shared契约升至1.4.0，新增Lobby角色和Lobby.Main体验；旧OpenWorld.Hub保留为兼容旧OpenWorld实例的别名、不进入活动目录。角色—体验映射由ServerCatalog生成到Go/C++，后端注册、分配、Agones标签和新玩家大厅落点已更新。
- Go 1.23.12容器内 `go test ./...`、`go vet ./...`、`go test -race ./...` 与Codegen `-check` 均通过。锁定版oapi-codegen 2.4.1无法生成现有OpenAPI 3.1规范的Go模型（其不支持规范中的nullable oneOf）；未写入客户端生成物。
- 插件目录迁移、DBA职责收敛、服务器Profile、UE编译/Cook/Stage及部署仍在后续任务中；本记录不代表整份设计基线已完成。

## 2026-09-21｜Foundation M0增量实施与原位阻断

- 按用户明确确认补齐00→03源码：正式薄主工程、Core、Data及Flow兼容扩展；旧流程公开入口保留，不创建第四个启动插件或新宿主。
- 新增显式FoundationStandalone配置、引擎内Maps/Probe/Flow生成脚本、三目标构建/开发Cook/受控进程/分项证据入口，维护项目节点与中文接口说明。
- 原位保留GamePlatformArena、MobaPresentation、DivineBeastsPresentation三个历史空描述，遵守用户“保留原位，记录构建阻断”的决定。正式Editor首次扫描退出6，未进入本批反射编译；不把历史临时宿主编译当M0验收。
- 原生算法、离线脚本及配置解析分开记录；UE三目标、真实四资产、Cook/Stage、三维与多PIE尚未通过。完整结果见FoundationM0Verification，不宣称可运行或完整游戏完成。
- 独立复核提出的数据调度、大小写身份及外部资源所有权问题纳入本批修复；源码/测试状态以执行进度和最后证据为准，不覆盖原失败记录。

## 2026-09-21｜插件编译续查

- 使用显式启用 ApplicationFlow 与 VFX、直接引用唯一正式源码的临时验证宿主，解决本次 VFX 构建入口的模块发现阻断；未改写正式游戏占位工程。
- 修正 VFX 世界子系统清理复合定时器时的只读句柄错误，补充中文说明；不改动公开接口。
- UE5.8.0 UHT 通过，ApplicationFlow／VFXClient／VFXEditor 三模块 C++ 编译通过；完整构建退出码 6，DLL 链接分别缺少引擎 Core／Projects／UnrealEd 库，未产出可加载插件 DLL。
- ApplicationFlow 原生 Debug／Release 各 21 个场景重新通过；没有执行 UE 自动化、Client／Server 构建或 Cook。同步更新 VFX 接入与验证说明。

## 2026-09-21｜历史工程参考与 ApplicationFlow

- 新增《神兽联盟历史对话来源索引》《神兽联盟历史对话与插件工程实现参考》，记录七个已读取会话的四十八轮消息、决策演变及后续插件职责。
- 新增 GamePlatformApplicationFlow 0.1.0 运行时代码、中文工程说明、原生行为测试及 UE 自动化测试代码。
- 同步维护总体规划、总体目录规划、插件规范及根规则中的参考入口；扩充现有工程文档首页。
- 本轮实际证据：生产调度核心 MSVC Debug／Release 编译成功，各运行 21 个行为场景。补充发现 F 盘 UE5.8 源码引擎并尝试独立插件验证；游戏主工程仍为空占位，不能宣称正式游戏验收。详见插件《测试与验证说明》。

## 2026-09-21

- 将根目录的普通文档迁入 `Docs/`，保留根级 `AGENTS.md` 作为工具规则入口。
- 新增后端五服务 Docker 本地开发部署、启动/停止脚本、自动配置校验和中文部署说明。
