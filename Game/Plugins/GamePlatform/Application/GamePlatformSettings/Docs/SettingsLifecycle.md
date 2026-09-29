# GamePlatformSettings 设置生命周期说明

## 1. Runtime 生命周期
`UGamePlatformSettingsSubsystem（游戏平台设置运行时子系统）` 使用 `UGameInstanceSubsystem（游戏实例子系统）`，跨登录、角色选择、Village（新手村）、OpenWorld（开放世界）、MainArena（主竞技场）和地图切换保持；不同 GameInstance／PIE 实例之间不共享可变状态。

初始化：注册模块化扩展监听 → 重建 Provider Registry（提供者注册表） → 读取当前端侧 Persistence Provider（持久化提供者） → 校验载荷 → 按 SchemaVersion 执行 Migration（迁移） → 解析配置层 → 发布 Snapshot（快照）。Runtime 为 0 Tick。

## 2. 客户端生命周期
User 层由 `UGamePlatformUserSettingsProfile` 保存稳定非权威设置，使用 `AsyncSaveGameToSlot` 异步保存。设备显示设置由 `UGamePlatformDeviceSettingsSubsystem` 直接适配 UE5.8 `UGameUserSettings`。

设备显示事务：`Stage → Preview/Commit → Confirm/Cancel`。Preview 不写盘；Confirm／Commit 才保存。普通 PIE 默认拒绝修改进程级设备设置，避免多个 PIE 实例互相污染。

## 3. 服务器生命周期
Server 只读解析顺序：`ServerDefault → Deployment → Environment → CommandLine → Session`。服务器不回写部署配置；非法关键值明确失败。

## 4. 账号切换与退出
Settings Runtime 不持有认证令牌或账号资料。账号切换前应完成或处理当前异步 User 层保存，清理账号相关 Provider／Session 覆盖后调用 `Reload()`。设备级 `UGameUserSettings` 属本机偏好，不因账号切换自动清空。

## 5. 销毁与异步边界
异步保存完成回调通过弱子系统引用回到游戏线程；子系统销毁后不再写回 UObject。Deinitialize 解绑 Modular Feature 委托并清空订阅、层数据和 Registry。
