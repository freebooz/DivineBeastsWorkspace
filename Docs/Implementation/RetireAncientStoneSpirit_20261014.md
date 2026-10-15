# SSLM 远古石灵退役记录（2026-10-14）

## 变更目标

按用户最新要求，从新手村开发体验移除静止训练敌人“远古石灵”（AncientStoneSpirit，旧怪物身份），包括原生角色、生成重生、客户端外观、专属蓝图/材质/字体、资源制作脚本、打包和启动开关及旧测试断言。保留 GamePlatformCombat（通用战斗）、GamePlatformAbilitySystem（通用技能）和三种开发法术的动画/特效。狼怪物（VillageWolf，新手村狼）的实现独立，不以远古石灵替换或合并。

## 已清理范围

- 双端：DivineBeastsDevelopmentMonster（开发石灵角色）及其内部测试；专服：DivineBeastsDevelopmentMonsterWorldSubsystem（生成/死亡重生）及测试；客户端：DivineBeastsDevelopmentMonsterVisualWorldSubsystem（视觉/生命文字）及测试。
- 删除石灵专属 Monolith（虚幻资产操作工具）原始制作与标签检测脚本；保留施法动画与VFX（视觉特效）的制作脚本。
- 开发客户端和开发专服配置移除 `bAllowDevelopmentMonsters`（开发怪物许可）、旧 `-DBADevelopmentMonsters`（运行参数）及石灵独占的 Cook（烘焙）白名单。
- 施法资产存在、配置与最终容器验证只要求施法资产，不再强制引入已退役石灵资源。
- 2026-10-13历史实施文档不删除，标记为旧证据，不把其旧测试和旧镜像视作退役后的验收结果。

## 仍需验收

- 确认石灵专属 `.uasset`（虚幻资产）文件真正移除、其他二进制资产无残余引用；在 UE5.8 Asset Registry（资源注册表）复核，清理 Redirector（重定向器）。
- 重跑静态搜索、Python（脚本）测试、PowerShell（构建脚本）解析以及最终 UE5.8 Editor/Client/Server（编辑器/客户端/专服）编译和 Village Cook，随后进入新手村确认无远古石灵实例。
- 仓库中其它未提交的水体、狼怪物、技能和界面任务归其他会话所有，不覆盖、重置或据其当前状态宣称测试通过。

**状态口径：** 只有全部门禁通过后，才能宣称“已完整移除并运行验收通过”。
