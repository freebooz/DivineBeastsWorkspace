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
