# DebugModel（调试模型）

FGamePlatformDebugSnapshot（调试快照）包含：
RequestId（请求编号）、CategoryId（分类编号）、TargetId（目标编号）、TargetGeneration（目标代次）、WorldGeneration（世界代次）、SourceView（来源视角）、Revision（修订号）、TimestampSeconds（时间戳）、Fields（字段）、Truncated（截断标记）和 FailureReason（失败原因）。

FGamePlatformDebugField（调试字段）包含：
Key（键）、DisplayName（显示名称）、ValueType（值类型）、Value（值）、Severity（严重度）、Sensitive（敏感标记）。

安全上限：
- 每个快照最多 128 字段。
- 单字段文本最多 512 字符。
- 快照估算文本负载最多 32K 字符。
- 单命令参数最多 256 字符。

DebugTarget（调试目标）只保存 Weak Actor/World（Actor/World弱引用）与稳定 DebugTargetId（调试目标编号），不序列化整个 UObject/Actor（虚幻对象/Actor）。World Travel（世界切换）后弱引用和世界身份使旧目标自然失效。