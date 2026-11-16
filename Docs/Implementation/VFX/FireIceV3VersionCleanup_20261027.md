# 火冰特效版本收敛：保留 V3，淘汰 V1/V2

用户于2026-10-27明确要求保留使用V3、删除V1/V2。本次范围为此前火冰审查涉及的原生资源；不将整个工程中所有名为V1/V2的独立功能一并删除。

## 变更

以下三个内容目录中的56份旧包已通过UE5.8 / Monolith原生删除接口移除：

- `/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1`：30份。
- `/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V2`：9份。
- `/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1`：17份，包含同目录内的V2掌心实验系统。

其中55份为淘汰资源；另外1份是V3仍在使用的冰晶网格，先迁移保留，再删除旧位置：

- 原位置：`/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V2/Meshes/SM_DBA_IceCrystal_V2`。
- 新位置：`/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/CastFlight/Meshes/SM_DBA_IceCrystal_V3`。
- `NS_DBA_IceCrystalFlight_V3`的Core Mesh Renderer显式改为新路径，并原生保存。未改变网格几何、尺寸、材质覆盖和运动参数。
- OBJ源模型按字节保留于插件 `SourceArt/VFX/OrbProjectile/Review/V3/SM_DBA_IceCrystal_V3.obj`，更新新网格的重导入来源。

V3原有48份资源全部保留，另增加迁入网格，共49份。其中包括原火冰掌心／飞行4个系统及后续掌心审核2个系统。其他独立技能的凝锋V2、定向飞弹V1及其共享Effect Type不在本次删除范围。历史源码、源素材和审核记录作为追溯资料保留，不能将历史作者工具重新执行后生成的旧目录视为当前版本。

## 删除前门禁与恢复

- 实际工程为`Game/DivineBeastsArena.uproject`，UE5.8.0；开始时无未保存地图或内容包。
- 原生AssetRegistry同时检查软／硬引用、管理引用、编辑器和游戏依赖。最初仅发现V3冰晶系统依赖旧网格；迁移后，56个旧包的范围外引用为0。
- 删除前加载引用方再确认，记录`LoadedReferencersBeforeDelete.json`，范围外引用仍为0。
- `Saved/Validation/FireIceV3Cleanup20261027/BeforeCleanup.zip`保留105份文件，逐项校验SHA256及ZIP完整性；包含全部被删资产、原V3文件及冰晶源OBJ。清单为`BackupManifest.json`。
- 首次常规原生删除返回0；随后在完整备份、严格路径允许列表和加载引用检查后，执行限定56个包的原生删除。调用等待超过服务响应时限；实际删除结果随后通过磁盘、AssetRegistry及版本差异独立核实，而不是根据超时回包推断成功。
- 不通过文件系统伪造或编辑uasset，没有关闭资源或网络一致性检查，没有清理账号、角色、服务器、Docker或构建缓存。

## 验证与边界

- 删除后原生读回：56个旧包全部不存在，三个空内容目录已由引擎移除。
- 原48份V3中仅冰晶飞行系统的网格路径发生磁盘变化，其余47份SHA256与备份一致。
- 验证阶段批量重载误包含当前审核地图，触发`EditorServer.cpp:1951 World Memory Leaks`保护并退出。无未保存手工场景。已改用新编辑器进程独立加载验证，不再重载当前地图包。
- 新进程12568独立启动原工程：49份V3资源全部加载成功；旧包数量0，V3对旧包引用0，项目表现包缺失依赖0。
- 6份保留Niagara系统重新同步编译均为0错误、0警告。只重载六个系统包清除编译产生的内存脏标记，未再次重载地图。冷启动与编译证据为同目录`NativeValidation.json`、`Diagnostics.json`。

本次是已授权的版本收敛与依赖修复，不代表按钮1/2已完成V3运行接入，也不代表客户端已经重新Cook、部署或人工视觉验收。此前审查中的正式播放定义仍属另外待完成的接入步骤。
