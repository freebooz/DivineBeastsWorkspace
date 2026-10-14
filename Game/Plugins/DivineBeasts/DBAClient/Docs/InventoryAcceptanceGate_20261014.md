# SSLM 背包系统联机交付剩余门禁（2026-10-14）

本文件基于2026-10-14的真实资产检查、Monolith（虚幻编辑器桥）蓝图回读、Cwebcodex（远程代码执行）构建日志及Go测试输出编制。执行记录与程序存在应区别；无法取得的生产运行结果必须判为待验证。

## 一、2026-10-14新增已确认事实

- `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/UI/Screens/WBP_DBA_UI_Inventory.uasset`（背包主界面原生蓝图）已实际存在，文件大小459822字节。Monolith即时返回父类`DivineBeastsInventoryScreen`（项目背包界面），`dump_blueprint_compile_log`（蓝图编译回读）显示0错误、0警告。
- 对`InventoryVisualAssets_20261013.json`（8类物品资源清单）中的8张`Texture2D`（纹理）和8份`DataAsset`（显示定义数据资产）执行逐项实际磁盘存在性检查，结果`items=8, icons=8, definitions=8, missing=0`。背包蓝图已增加44个命名图标控件，总控件216个。**纹理文件存在及蓝图编译通过不是实际PC移动设备视觉、交互或Cook（烘焙）验收。**
- 已有`DivineBeastsArenaServer Win64 Development`（Windows专用服务器开发版）完整构建日志于最近一次`wc_job_CVg3sq9VgQ47JJLO`记录`Result: Succeeded`，输出`Game/Binaries/Win64/DivineBeastsArenaServer.exe`。不能把另两次带预编译清单的失败构建当成同一次成功或忽略其他平台/目标构建。
- 实际执行`go test -count=1 ./internal/modules/inventory ./internal/transport/httpadapter ./internal/app/gateway`（Go背包、网络适配及网关测试），3包通过。实际执行`-tags=productiondeps`及`-tags=productiondeps,grpcdeps`（数据库生产依赖和gRPC依赖组合）测试，背包/HTTP/PG测试包通过；这些Go测试**不是**真实PostgreSQL迁移执行。
- `DivineBeastsUIClient`（项目客户端界面模块）曾因并行编写的`DivineBeastsPartyScreenBinding.cpp`（组队界面适配器）及其他组队界面的`TObjectPtr`（UE对象持有类型）用法报C3535/C2440，不能据此推断背包源码报错。本轮正在运行专门的UI模块统一构建`wc_job_XUsp2TP008ZRpeE-`，应以该任务最终退出码为准。

## 二、真实剩余可玩门禁

1. GameServer当前`validate-transfer`（迁移票据验证）与`session_reserve→session_claim→session_commit`（玩家准入预留、领取、提交）链路尚无同一运行会话的完整证据。数据库`session_bindings`（已提交玩家会话绑定）和`SessionEpoch`（连接世代）不能只凭请求体文字信任。
2. 已新增`000012_player_inventory_session_lease_renewal.sql`（已绑定会话续租迁移）及专服租约续期端口；续租**不能代替**首个session_commit或复活已经失效的会话。需要在真实PostgreSQL隔离库运行全部迁移并做旧Boot、旧Epoch、断线重登、会话过期验证。
3. 世界内采集目标的距离、命中、资格、剩余次数、独占预留及权威来源事件ID仍须与`SubmitWorldPickup`（服务器拾取事件出口）真正接线；任务系统以已持久化CompletionID（完成身份）对接`SubmitQuestReward`（任务奖励出口），避免未提交任务被提前发奖。
4. `Inventory.Consumed`（真实库存扣除事实）应由只执行一次的GAS（玩法能力系统）或业务效果消费者使用，并通过`sourceEventId`（来源事件身份）和`itemInstanceId`（物品实例身份）去重；尚无药露实际恢复生命/满血拒绝使用的专服游戏测试证据。
5. 必须复现“登录→选择/创建角色→新手村→I键打开背包→采集和任务发奖→拆分/移动/合并→快捷栏→使用物品→注销重登”全链路；Dedicated Server两客户端并发、Windows客户端Cook、最小Server Cook及真实视觉人工验收仍待完成。

## 三、验收判定

**已完成：** 原生纹理及数据资产存在性、背包Widget编译回读、后端相关Go测试、专服一次完整构建成功。

**未完成：** 首个玩家准入绑定、正式PostgreSQL迁移与数据库测试、游戏世界采集/任务/使用效果的端到端可靠链路、双客户端、Cook及实际画面人工审查。

不得在上述未完成项目关闭之前把背包整体称作已可玩或已正式交付；不要伪造通过证据。
