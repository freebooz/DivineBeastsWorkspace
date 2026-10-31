# 已退休资源脚本（2026-10-31）

用户明确要求先清理资源、鸟群和地形相关过期脚本。这里只归档有真实替代入口或已完成一次性恢复的3项；不按文件日期或“没有文件名引用”批量删除工具。

| 归档文件 | 退休依据 | 当前入口 |
| --- | --- | --- |
| AuthorBirdFlockAssets.py | 写入七项旧原型身份；当前运行已改为Flight，旧写入不可恢复 | Tools/Unreal/VFX/BirdFlock/ImportBirdFlockFlightMeshes.py；材质/系统使用既有Monolith作者 |
| ValidateBirdFlockAssets.py | 校验首版_Cm网格，不能作为当前Flight验收 | Tools/Unreal/VFX/BirdFlock/ValidateBirdFlockP4Assets.py、AuditBirdFlockDelivery.py |
| ResumeTerrainV3FormalCover.ps1 | 只适用九层完成、唯一Dirt待办；真实AppliedManifest已为10/10，此临时恢复不再适用 | Tests/Assets/TestTerrainColorV3Policy.py、Tests/Architecture/TestTerrainV3FormalCover.py、当前Monolith冷读 |

原实现体保留用于追溯，入口立即拒绝执行；不可从归档恢复退休资源或旧绑定。没有运行脚本继续调用以上路径。历史文档/来源清单保留当时事实，不伪造为新路径；当前索引与测试已更新。归档前后尺寸/哈希及141项范围清单见工程Saved/Validation/ResourceOwnershipRemediation20261031。
