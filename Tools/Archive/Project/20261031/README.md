# 全项目已完成/重复脚本归档（2026-10-31）

全项目898项维护脚本及4697个文本文件的调用关系审查后，新增4项可恢复归档。原始字节、原路径和当前归档SHA见`Tools/Archive/ScriptRetirementRegistry_20261031.json`，原字节备份保留在工程Saved。这里只保存已完成的一次性实现或字节完全相同的重复入口；每项在导入Unreal/注册回调之前拒绝执行。

| 文件 | 退休证据 | 当前职责 |
| --- | --- | --- |
| PromoteVillageForestReview.py | 固定原地图SHA的一次性升正式操作已完成；正式图与原始哈希不同，禁止再追加ForestPaths | 正式Village及既有森林资源继续维护；后续修改使用新的审核基线 |
| CapturePalmRedesignProbeUE58.py | 与保留的CapturePalmRedesignReviewUE58.py逐字节一致，无独有行为 | Review及DualReview保持唯一当前采集入口 |
| ProbeFireballEditorAutoExec.py | 八tick入口能力探针已有completed=true，且无维护区调用方 | 正式Monolith/Editor Python执行和当前采集工具 |
| BindManualPCGRealizedMonolith.py | 冻结收据一次性绑定已完成，九节点九边、Revision2独立冷读及当前Profile SHA一致 | 当前Profile/原生图和现行作者/验证器；不重放旧绑定 |

与`Tools/Archive/Resources/20261031`三项合计7项、原始45548字节。没有实际删除源码、原生资产、来源美术或历史证据。238项无直接文件名/模块引用的脚本不能据此判死；退休兼容stub仍被唯一SpellPresentation合同和测试消费，保留。构建、部署、协议生成、测试自动发现及第三方原版脚本未纳入删除范围。

静态引用检查不能穷尽外部任务计划、个人命令或二进制字符串；如确需恢复，先核对当前Owner和替代入口，再使用登记的原始备份，不从归档执行旧写入逻辑。
