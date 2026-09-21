# Lifecycle（生命周期）

实例状态：

`Requested → Loading → Spawning → Active → Completed`

异常分支：`Cancelled / Failed`。

- 异步 Definition 与 Niagara 加载句柄保存在实例记录中。
- Stop / Deinitialize 会 Cancel / Release 自己持有的句柄。
- `Generation` 防止旧 World 的 Handle 操作新 World。
- WorldSubsystem 只在非 Dedicated Server 世界创建。
