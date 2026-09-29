# GamePlatformSave 性能与安全设计

版本：0.2.0｜2026-09-29

## 1. 性能原则

`GamePlatformSave` 不属于帧循环系统，目标是低频、事件驱动、可预测。

当前约束：

- 0 Tick；
- 0 Ticker；
- 热路径不扫描 `Saved` 目录；
- 热路径不读取文件；
- 文件 Read/Write/Delete 全部在线程池执行；
- Provider 注册、迁移、完成回调仅在 Game Thread；
- 单记录 Payload 上限 8 MiB；
- 同 Key 同时最多一个 IO；
- 全局并发请求上限 16；
- 文件路径由 Key 的稳定摘要直接定位，不执行目录搜索。

## 2. 为什么不使用每帧自动保存

每帧或高频自动 Save 会导致：

- SSD/闪存无意义写放大；
- 游戏线程或 IO 线程突刺；
- 两次状态变化之间产生大量中间版本；
- 崩溃恢复语义难以判断。

推荐由业务层在明确事件触发：

```text
页面关闭
本地草稿明确提交
关键非权威偏好变化后防抖
应用安全退出
```

本插件不自行猜测保存时机。

## 3. 原子替换策略

写入采用同目录唯一临时文件：

```text
<id>.gpsav.tmp.<guid>
```

步骤：

1. OpenWrite；
2. 写入完整 Envelope；
3. `Flush(true)`；
4. 原主档存在时先复制到 `.bak.tmp.<guid>`；
5. Move 替换 `.bak`；
6. Move 临时主档替换正式主档。

这显著降低“直接覆盖主文件”导致的半写文件风险。

需要注意：文件系统、操作系统缓存、突然断电和硬件控制器行为仍可能影响绝对持久性，因此文档使用“应用级原子替换/最佳努力持久化”，不声称具备数据库 WAL 等级事务保证。

## 4. 损坏检测

Envelope 中保存 CRC32。

CRC32 能发现常见随机损坏，但不能证明文件来自可信来源，也不能阻止有意篡改。

因此：

> 本地存档内容无论 CRC 是否正确，都不得被服务器当作权威输入直接接受。

## 5. 路径安全

原始 `Namespace/ProfileKey/SlotName` 不进入最终文件名。

插件先计算：

```text
SHA-1(Namespace + ProfileKey + SlotName)
```

最终只使用 40 字符十六进制摘要生成路径。

这同时降低：

- 路径穿越；
- 非法文件名；
- ProfileKey 原文出现在目录列表中的风险。

SHA-1 在这里仅作为稳定摘要，不用于密码学认证。

## 6. 敏感数据

禁止写入：

- Password（密码）；
- AccessToken / RefreshToken；
- Session Secret；
- 私钥；
- 后端服务凭据；
- 支付凭据；
- 任何本应由系统安全存储托管的秘密。

当前实现没有加密承诺，因此不能把“文件不可读”作为安全目标。

## 7. 防作弊边界

插件不做反作弊真源。

即使用户修改：

```text
Saved/GamePlatformSave/*.gpsav
```

也不能获得：

- 金币；
- 装备；
- 等级；
- 任务奖励；
- 排位分；
- 权益；
- 服务器世界状态。

这些数据必须由服务器／Go 后端重新验证。

## 8. 账号隔离

`ProfileKey` 只决定本地存档隔离。

Save 不依赖 `GamePlatformOnline`，也不缓存当前账号。账号切换后，组合层必须使用新主体对应的 ProfileKey。

这避免：

```text
Save → Online → Save
```

横向循环依赖。

## 9. 线程安全

- Subsystem 内部请求表、Provider 表和诊断只在 Game Thread 访问；
- Storage/Policy 后台阶段只处理纯值和文件 API，不访问 UObject；
- 完成结果通过 `AsyncTask(ENamedThreads::GameThread, ...)` 返回；
- Provider 迁移只在 Game Thread；
- GameInstance 销毁使用 ScopeId/Generation 防止旧完成结果进入新生命周期。

## 10. 后续性能验证

生产前建议通过 Unreal Insights（虚幻性能分析）或可重复压测补证：

- 8 MiB 极限记录写入延迟；
- 多 Key 16 并发 IO 峰值；
- 机械盘／SSD／移动闪存差异；
- 低磁盘空间；
- 只读目录；
- 崩溃／断电模拟。

没有实测证据前，不继续为理论性能增加复杂缓存或线程池体系。
