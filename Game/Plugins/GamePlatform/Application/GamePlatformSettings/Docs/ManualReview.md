# GamePlatformSettings 人工审核

## 1. Runtime / Provider

1. 启用测试 Provider，确认 Descriptor 数量和 Provider 数量正确。
2. 重复 ProviderId 或 SettingId 应明确失败。
3. 修改 User 层但不 Apply，Snapshot 最终值不应改变。
4. Apply 后只广播一个批量 ChangeSet。
5. 注销 Provider 后 Reload，应从 Snapshot 移除其 SettingId。
6. 敏感设置诊断不得输出真实值。

## 2. 客户端设备设置

### 暂存
读取设备快照 → Stage 合法值 → 确认窗口未变化 → Discard 恢复候选。

### 预览取消
Stage → Preview → 确认无写盘 → Cancel → 分辨率、窗口模式、VSync、帧率、画质全部恢复 → 重启仍保持旧已保存值。

### 预览确认
Stage → Preview → Confirm → 重启 → 新设置保持。

### 非法值
0x1080、超大分辨率、NaN/Inf、负帧率、画质 -1/5 均应失败且不改变当前设置。

## 3. User Profile / Migration

1. 设置 User 层后 Apply + Save。
2. 关闭并重开客户端，Reload 后值一致。
3. 使用旧版本 Profile 验证逐版本迁移。
4. 人为制造迁移失败，确认原 Profile 不被覆盖。
5. 损坏 Profile 应回退默认值并保留原文件供诊断。

## 4. Server

对 OpenWorld／Village／MainArena 分别验证：

- ServerDefault INI。
- Deployment INI。
- GP_SETTING_* 环境变量。
- -GPSetting.<SettingId>=... 命令行。
- 优先级符合设计。
- 类型／范围非法时启动配置解析失败。
- Server 产物不包含 GamePlatformSettingsClient／Editor、UMG、Slate、音频或输入依赖。

## 5. 生命周期与性能

1. 登录→角色选择→Village→OpenWorld→MainArena→OpenWorld，Snapshot 不因地图切换丢失。
2. 快速拖动设置 UI 只修改内存草稿，不连续写盘。
3. 运行分析确认 Settings 没有 Tick/Ticker。
4. Provider 拓扑变更、Reload、Apply、Save 不产生无限递归或重复广播。
5. 多 PIE 不污染真实用户 Profile 或设备设置。

人工审核必须记录引擎版本、平台、目标、步骤、预期、实际结果和异常。

