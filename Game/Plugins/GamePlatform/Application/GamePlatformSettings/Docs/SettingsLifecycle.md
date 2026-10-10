# GamePlatformSettings 设置生命周期说明

## 1. Runtime 生命周期
`UGamePlatformSettingsSubsystem（游戏平台设置运行时子系统）` 使用 `UGameInstanceSubsystem（游戏实例子系统）`，跨登录、角色选择、地图与体验切换保持；不同 GameInstance／PIE 实例之间不共享可变状态。

初始化：注册模块化扩展监听 → 重建 Provider Registry（提供者注册表） → 读取当前端侧 Persistence Provider（持久化提供者） → 校验载荷 → 按 SchemaVersion 执行 Migration（迁移） → 解析配置层 → 发布 Snapshot（快照）。Runtime 为 0 Tick。

## 2. 客户端生命周期
User 层由 `UGamePlatformUserSettingsProfile` 保存稳定非权威设置，使用 `AsyncSaveGameToSlot` 异步保存。设备显示设置由 `UGamePlatformDeviceSettingsSubsystem` 直接适配 UE5.8 `UGameUserSettings`。

设备显示事务：`Stage → Preview/Commit → Confirm/Cancel`。Preview 不写盘；Confirm／Commit 才保存。普通 PIE 默认拒绝修改进程级设备设置，避免多个 PIE 实例互相污染。

## 3. 服务器生命周期
Server 只读解析顺序：`ServerDefault → Deployment → Environment → CommandLine → Session`。服务器不回写部署配置；非法关键值明确失败。

## 4. 账号切换与退出
Settings Runtime 不持有认证令牌或账号资料，也不依赖 Online。登录组合层只通过 `SwitchUserContext(UserContextKey)` 注入不透明稳定键；Client Persistence 使用 SHA-1 派生本地 SaveGame 槽名，原始账号键不写入文件名或普通日志。

切换用户上下文前若存在待 Apply、User Dirty 或异步 Save，Runtime 返回 `SettingsUserContextSwitchBlocked`，调用方必须先完成 `Apply → Save → 等待保存完成`。切换成功后自动 Reload 新用户档案。Logout 传空键：不再读取／保存 User Profile，并回到默认／项目／Provider／Session 配置链。设备级 `UGameUserSettings` 属本机偏好，不因账号切换自动清空。

## 5. 销毁与异步边界
异步保存完成回调通过弱子系统引用回到游戏线程；子系统销毁后不再写回 UObject。Deinitialize 解绑 Modular Feature 委托并清空订阅、层数据和 Registry。

若 Provider 拓扑在异步 Save 期间变化，Runtime 不立即 Reload；只在当前代次保存成功后处理待重载，保存失败或期间出现新 Mutation 时继续延后，防止丢失未保存设置。

## 2026-10-09 读取生命周期补充

用户档案读取采用BeginLoad受理和完成事件，不在游戏线程同步读取磁盘；真实平台存在性与LoadGame均由UE原生异步系统调度，所有UObject转换及Runtime完成在游戏线程。候选数据不对外可见，完整成功才发布。bLoading标记在飞状态，失败保留已发布视图；初始化读取失败仍明确记录错误，不声明档案已加载。

读取重载/用户换绑/拓扑卸载/退出均用读取代次隔离；读取在飞期间普通Set/Save/Reload拒绝，而经校验的用户换绑会失效旧消费代次并先撤销旧User层。控件只通过事件更新，Runtime仍无业务Tick。

### 2026-10-09 同步终态与拓扑撤回

拓扑事件先失效旧读取候选再重建，即使重建失败或广播内延后处理，旧Completion不能发布。保存拥有独立SaveRequestGeneration及实例代次，外部BeginSave前登记在飞与终态身份，同步、重复、启动失败及迟到结果只能消费一次。持久化克隆采用GI持有的共享所有权，外部调用栈保活副本与纯值参数，退出只撤销服务消费资格。账号默认/Session投影广播后检查关闭与实例代次，监听器关闭服务后不得启动新读取。

新增AsyncLoadGeneration与SaveTerminalGate用例使用实际Runtime入口和手控测试Provider；UE自动化尚未运行，不能据此声称行为已经通过。

### 准备失败的事件所有权

`ReloadInternal`统一拥有注册表重建和持久化Provider解析失败的终态发布。直接Reload、ProviderChanged以及广播内延后拓扑唤醒均发布一次非Loading失败快照；外层拓扑调用方仅记诊断，不重复发布。失败保留上次成功值和持久层，订阅者无需轮询GetSnapshot才能结束加载投影。

准备阶段捕获实例Generation与LoadGeneration。每次外部重建、Provider资格、克隆或上下文调用后复核；关闭或新候选接管时旧准备栈返回SettingsPreparationInvalidated，不能覆盖新候选。无可选持久化Provider仅清除Scoped/Factory，不在解析器内再次推进LoadGeneration；默认层候选仍能完成。

PreparationFailureEvent覆盖实际订阅者看到注册表/持久化解析失败且只通知一次；DeferredPreparationEvent以真实GT异步唤醒覆盖广播内延后路径。均为UE测试源码，未由本分工执行。
