# 逐插件审查与当前整改状态（2026-10-09）

当前覆盖62个插件身份、83个插件模块（原审查82，主线既有技能模块整合后83）；GamePlatform稳定身份40个（物理平台39＋MOBA Arena）。修改前设计/性能/规范保留在Audit/PluginReviewMatrix.json与PluginAudit.md；61个原审查编号逐项保存在FindingDisposition.json，独立追加问题见IndependentReviewResults.md及领域报告。

源码整改与模块构建不能证明全部插件设计完善或达到最佳工程实践。批准配置、地图、真实资源、后端与阶段能力缺口保留真实条件；CPU/GPU/内存收益无设备实测，全量中文存量未逐行认证。实际检查与目标构建分别见ValidationSummary.json、UEBuildResults.json。

| 插件稳定身份 | 模块 | 审查编号 | 当前源码/设计处置 |
|---|---:|---|---|
| `DBAArena` | 3 | G-01、GW-02、GW-04、GW-15、GW-16、GW-17 | G-01：真实依赖声明与静态闭包已修；服务器45/客户端65模块构建通过，编辑器结果另表；GW-02：源码装配与真实死亡事实桥接已补；完整UE/地图/联机未验收；GW-04：重连与死亡复活受理源码已补；真实到期Spawn/Possess待UE；GW-15：整合基线已消除；双World实机待验证；GW-16：缺批准配置/地图；正确NotConfigured保持；GW-17：本轮源码修复；Travel/迟到复制待UE验证 |
| `DBAClient` | 5 | APP-11、F14、F15、F16、F17、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；APP-11：本轮源码已修；真实UE行为回归待执行；F14：真实Data事务源码已实施；UE资源/回调待验收；F15：格式/失败合同已修；真实默认资源仍缺；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验；F17：本轮触及范围已补伴随说明；全量存量不在本组验收 |
| `DBAContentPack_Common` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAFrontEndPack` | 0 |  | 重点源码/消费者审查未发现确认问题；并非全量完备或性能认证，阶段见修改前审查 |
| `DBAGameplay` | 3 | GW-02 | GW-02：源码装配与真实死亡事实桥接已补；完整UE/地图/联机未验收 |
| `DBAHeroPack_Boar` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Dog` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Dragon` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Goat` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Horse` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Monkey` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Ox` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Rabbit` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Rat` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Rooster` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Snake` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAHeroPack_Tiger` | 0 | F16、G-02 | G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验；F16：主执行者已修服务器声明根/模块与Stage规则；实际干净Cook/Stage产物未验 |
| `DBAServer` | 1 | G-02、R-08 | R-08：世界退休与排空源码已修；三角色运行待UE；G-02：Server声明根/模块与Stage规则已修；干净Cook产物未验 |
| `DBAUIPack_Core` | 0 |  | 重点源码/消费者审查未发现确认问题；并非全量完备或性能认证，阶段见修改前审查 |
| `DBAWorldPack_Village` | 0 |  | 重点源码/消费者审查未发现确认问题；并非全量完备或性能认证，阶段见修改前审查 |
| `DBAWorlds` | 1 | G-01 | G-01：真实依赖声明与静态闭包已修；服务器45/客户端65模块构建通过，编辑器结果另表 |
| `GamePlatformAbilitySystem` | 1 | GW-01、GW-02 | GW-01：整合基线已消除；真实GAS运行待UE验证；GW-02：源码装配与真实死亡事实桥接已补；完整UE/地图/联机未验收 |
| `GamePlatformAI` | 2 | GW-12 | GW-12：目标声明/服务器闭包已修；全量中文历史存量未审完，运行产物另验 |
| `GamePlatformAnimation` | 2 | GW-12、GW-13 | GW-12：目标声明/服务器闭包已修；全量中文历史存量未审完，运行产物另验；GW-13：授权保留的未实现能力；不计功能完成 |
| `GamePlatformApplicationFlow` | 1 | APP-13 | APP-13：原生策略回归和源码复核完成；真实UE行为回归待执行 |
| `GamePlatformArena` | 5 | GW-03、GW-04、GW-11、GW-12 | GW-03：本轮源码接线；待真实握手/后端联调；GW-04：重连与死亡复活受理源码已补；真实到期Spawn/Possess待UE；GW-11：部分接口复用；完整Experience迁移待设计/资产评审；GW-12：目标声明/服务器闭包已修；全量中文历史存量未审完，运行产物另验 |
| `GamePlatformCamera` | 1 |  | 重点源码/消费者审查未发现确认问题；并非全量完备或性能认证，阶段见修改前审查 |
| `GamePlatformCharacter` | 1 | GW-02 | GW-02：源码装配与真实死亡事实桥接已补；完整UE/地图/联机未验收 |
| `GamePlatformCombat` | 1 | GW-02 | GW-02：源码装配与真实死亡事实桥接已补；完整UE/地图/联机未验收 |
| `GamePlatformCommerceUI` | 1 | APP-06、APP-15 | APP-06：本轮源码已修；真实UE行为回归待执行；APP-15：本轮触及范围中文说明已审核；全量历史存量未完成审核 |
| `GamePlatformCore` | 1 |  | 重点源码/消费者审查未发现确认问题；并非全量完备或性能认证，阶段见修改前审查 |
| `GamePlatformData` | 2 | R-01、R-02 | R-01：源码已修；真实多实例资源与GC待UE；R-02：源码改为按需低频与世界事件；性能收益未实测 |
| `GamePlatformDebug` | 2 | G-04 | G-04：触及范围补中文；全量存量人工审核未完成 |
| `GamePlatformDeveloperTools` | 1 | G-04、R-07 | R-07：真实定义审计已修；生产性能场景执行器仍缺批准场景；G-04：触及范围补中文；全量存量人工审核未完成 |
| `GamePlatformEntitlement` | 2 | APP-06、APP-12、APP-15 | APP-06：本轮源码已修；真实UE行为回归待执行；APP-12：本轮源码已修；真实UE行为回归待执行；APP-15：本轮触及范围中文说明已审核；全量历史存量未完成审核 |
| `GamePlatformEquipment` | 3 | APP-07、APP-08、APP-15 | APP-07：已复核既有修复并补本轮边界；真实UE行为回归待执行；APP-08：本轮源码已修；真实UE行为回归待执行；APP-15：本轮触及范围中文说明已审核；全量历史存量未完成审核 |
| `GamePlatformGameplay` | 1 | GW-02、GW-11、GW-14 | GW-02：源码装配与真实死亡事实桥接已补；完整UE/地图/联机未验收；GW-11：部分接口复用；完整Experience迁移待设计/资产评审；GW-14：本轮文档修复 |
| `GamePlatformInput` | 1 | APP-01、APP-02、APP-03 | APP-01：已复核整合后的既有源码；实际UE行为仍待运行；APP-02：已复核现有合同与失败边界；正式后端/资产集成仍待验证；APP-03：本轮源码已修；真实UE行为回归待执行 |
| `GamePlatformInteraction` | 1 | GW-09 | GW-09：Custom及终态广播重入源码修复；UE行为回归未执行 |
| `GamePlatformInventory` | 1 | APP-06、APP-15 | APP-06：本轮源码已修；真实UE行为回归待执行；APP-15：本轮触及范围中文说明已审核；全量历史存量未完成审核 |
| `GamePlatformLiveOps` | 1 | APP-10、APP-15 | APP-10：本轮源码已修；真实UE行为回归待执行；APP-15：本轮触及范围中文说明已审核；全量历史存量未完成审核 |
| `GamePlatformLoading` | 1 | APP-05 | APP-05：已复核整合后的既有源码；实际UE行为仍待运行 |
| `GamePlatformLobby` | 3 | GW-12、GW-13 | GW-12：目标声明/服务器闭包已修；全量中文历史存量未审完，运行产物另验；GW-13：授权保留的未实现能力；不计功能完成 |
| `GamePlatformLocalization` | 1 |  | 重点源码/消费者审查未发现确认问题；并非全量完备或性能认证，阶段见修改前审查 |
| `GamePlatformNavigation` | 2 | GW-07、GW-12 | GW-07：在飞重复ID基线已修；追加完成后复用ID的旧取消句柄修复；GW-12：目标声明/服务器闭包已修；全量中文历史存量未审完，运行产物另验 |
| `GamePlatformOnline` | 2 |  | 重点源码/消费者审查未发现确认问题；并非全量完备或性能认证，阶段见修改前审查 |
| `GamePlatformPCG` | 2 | GW-08 | GW-08：整合基线已消除；原生规则通过/真实129次PCG待验证 |
| `GamePlatformPresentation` | 2 | F01、F02、F17 | F01：已合入旧修复复核；六字段具体度补修；UE待验收；F02：旧消歧修复复核；诊断与只读预检补齐；UE待验收；F17：本轮触及范围已补伴随说明；全量存量不在本组验收 |
| `GamePlatformProgression` | 2 | APP-09、APP-12、APP-15 | APP-09：已复核既有修复并补本轮边界；真实UE行为回归待执行；APP-12：本轮源码已修；真实UE行为回归待执行；APP-15：本轮触及范围中文说明已审核；全量历史存量未完成审核 |
| `GamePlatformQuest` | 3 | GW-05、GW-06、GW-12 | GW-05：追加修复完整快照与按任务实例重放；UE行为回归未执行；GW-06：整批幂等与广播重入源码修复；生产Quest端口缺交付；GW-12：目标声明/服务器闭包已修；全量中文历史存量未审完，运行产物另验 |
| `GamePlatformSave` | 1 | APP-04 | APP-04：已复核整合后的既有源码；实际UE行为仍待运行 |
| `GamePlatformServer` | 1 | R-03、R-04 | R-03：Provider/子系统生命周期与重入源码已修；真实握手待联调；R-04：流式接收预算已修；压力/超时待网络 |
| `GamePlatformSession` | 1 |  | 重点源码/消费者审查未发现确认问题；并非全量完备或性能认证，阶段见修改前审查 |
| `GamePlatformSettings` | 4 | APP-14 | APP-14：本轮源码已修；真实UE行为回归待执行 |
| `GamePlatformSFX` | 1 | F06、F07、F08、F17 | F06：旧修复已覆盖；原生回归通过；UE待验收；F07：旧先监听修复及真实引擎源码已复核；实播待验收；F08：公开终态合同及去重失败已实施；UE待验收；F17：本轮触及范围已补伴随说明；全量存量不在本组验收 |
| `GamePlatformSurface` | 2 | F09、F10、F17 | F09：实际内容缺口保留；本轮不生成资产；F10：源码已实施；新旧代/取消回归待UE；F17：本轮触及范围已补伴随说明；全量存量不在本组验收 |
| `GamePlatformTelemetry` | 1 | G-04、R-05、R-06 | R-05：HTTP持有/预算及Sink/上下文重入源码已修并独立窄复核；服务器模块已编译，UE行为/真实请求未运行；R-06：Public边界已收窄并保留不透明兼容；构造/析构完整类型修正已被服务器模块编译；G-04：触及范围补中文；全量存量人工审核未完成 |
| `GamePlatformUI` | 1 | F11、F17 | F11：旧资源所有权修复复核；事件时序补齐；UE待验收；F17：本轮触及范围已补伴随说明；全量存量不在本组验收 |
| `GamePlatformVFX` | 2 | F03、F04、F05、F08、F17、G-01 | G-01：真实依赖声明与静态闭包已修；服务器45/客户端65模块构建通过，编辑器结果另表；F03：生产评分回归红转绿；UE Registry待验收；F04：旧依赖门禁复核；根树生命周期补齐；UE待验收；F05：源码回退与资源租约已实施；实际资产待验收；F08：公开终态合同及去重失败已实施；UE待验收；F17：本轮触及范围已补伴随说明；全量存量不在本组验收 |
| `GamePlatformVillage` | 3 | GW-12、GW-13 | GW-12：目标声明/服务器闭包已修；全量中文历史存量未审完，运行产物另验；GW-13：授权保留的未实现能力；不计功能完成 |
| `GamePlatformWorld` | 2 | GW-10 | GW-10：缺交付条件；正确失败关闭保持 |
| `MobaPresentation` | 2 | F12、F13、F17 | F12：源码已实施；UE映射回归待执行；F13：引擎事件接线已实施；UE/联机待验收；F17：本轮触及范围已补伴随说明；全量存量不在本组验收 |
