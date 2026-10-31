# 资源归属整改实施计划（2026-10-31）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 原生迁移175包，收敛植物/地形归属并隔离历史/样例，保留现有效果。
**Architecture:** 同一正式工程、现有内容包、AssetTools RenameAssets；单一路径合同连接作者、校验与发布，绝不复制运行逻辑。
**Tech Stack:** UE5.8、Monolith、Python、C++、PowerShell、原生AssetRegistry与IoStore。
**Spec:** ResourceOwnershipDesign_20261031.md及已获用户批准的Saved/Validation/ResourceDirectoryAudit20261031/ResourceDirectoryAudit.txt。

## Global Constraints

- 当前main原位执行，保留既有改动，不提交/推送；不擅自停止无关进程，不清持久数据。
- 不新建插件，不改变源RGB、模型尺度、材质语义、世界布局、协议或主资产身份。
- 真实资产操作只经Monolith，未保存目标不覆盖，全部输出在当前工程Saved。
- 固定Win64/Development、NoSharedPCH、NoUBA、MaxParallelActions 8；Editor另UsePrecompiled及AdditionalPlugins DBAServer。
- 调试t.MaxFPS 0/r.VSync 0；客户端验证前核对真实Village/MainArena版本与进程。

## Review Focus

- 目标路径冲突、同包多导出、大小写重复与中途恢复不得覆盖人工资产。
- 重定向清理不得跟随目标误删真实资源；零原生引用不等于没有文本/动态引用。
- SourceArt历史来源不能被批量替换成伪造历史；只改当前作者和消费者路径。
- 地图/材质/纹理/网格外观及层级不得因Owner迁移变化。
- 样例/历史/纯表现不得进入专服；审核地形仅显式演示配置允许。

### Task 1：合同、失败回归与保护基线

Files: Tools/Unreal/Resources/{ResourceOwnershipContract.py,ResourceOwnershipManifest.json,ResourceOwnershipMonolith.py}; Tests/Assets/TestResourceOwnershipContract.py。
Interfaces: load_contract/validate_contract返回175项source/target/group；native baseline、move_group、verify和cold_read从同一合同读取。
- [ ] 写合同冲突/非法路径及源旧引用阻断测试，观察RED，再实现有限合同校验。
- [ ] 原生清洁检查、引用快照、完整资产备份及保效指纹/截图；真实基线对归属断言RED。

### Task 2：公共植物与地形归属

Files: 上述合同/Monolith作者；VillageGrassWorldSubsystem.cpp、VillageSnowSettings.cpp及其Tests；两客户端INI、TerrainV3ColorMapping及当前地形/花草作者/验证器。
- [ ] 原生迁移51在用花草与40样例、12地形审核包与10公共色彩图，逐组保存/回读。
- [ ] 同步代码/配置/作者路径，保护源图哈希与效果指纹；失败原样留证，Native归属验证转GREEN。

### Task 3：鸟历史、StyleHex与发布目录

Files: 原生7旧鸟网格、55StyleHex样例；DefaultGame.ini、PluginDemoClient/DefaultGame.ini、ServerPresentationExclusions.json及生成INI；Tests/Assets资源与包验证。
- [ ] 先验证当前Flight资产指纹和引用，原生迁移历史/样例，不改变当前系统。
- [ ] 完成有限重定向引用修复、源路径清理与发布阻断；保持既有兼容对象除非明确完成引用迁移。
- [ ] 最小离线/原生回归与独立进程冷读，保效指纹、地图布局和新Owner图通过。

### Task 4：构建与实际发布验证

Files: 既有BuildFoundation/CookFoundation/CookFrontEndClient及必要有限容器检查。
- [ ] 仅最终源输入执行完整Editor/Client/Server增量构建；本轮RHI颜色证据有效则复用。
- [ ] 实际Client/Server/11图演示Cook/Stage，检查新路径唯一导出、旧路径/历史/样例拒绝及专服CPU数据保留。
- [ ] 如启动前端验证，先绑定新Stage、共享合同、真实Village/MainArena路径/PID，受控更换本轮旧实例。

### Task 5：文档、一次fresh审查与交付

Files: Foliage README、相关Owner文档、总体目录规划、ResourceOwnershipDelivery_20261031.md。
- [ ] 更新原型/当前/历史、来源与唯一入口；资产/源码/构建/Cook/运行/人工分别报告。
- [ ] 一次fresh整体审查；Important补失败回归后一轮修复，不派复审。

## Execution ledger

2026-10-31 用户已批准审查报告并明确要求按序原生整改；沿用Native执行。原main有7项既有修改/报告，全部保留。编辑器PID7028绑定正式工程，当前无PIE、脏地图0、脏内容0。仅该项目AGENTS.md生效。
Ruling: 地形三代审核材质整体迁到Village开发目录而不重编材质图 — 已知都硬绑定Village资源，且当前正式包不包含这些母材质 — 后续若要跨世界复用，应另将参数/默认贴图抽象，不能再把整个世界绑定母材质当公共资源。
Ruling: 在用供应商闭包51包与40样例分开，历史鸟和StyleHex只迁移不删除 — 避免失去原稿/历史 — 目录仍保留开发资产体积，但正式发布须拒绝。

Task1：合同五项回归先因缺模块RED，再全部GREEN；实际原生归属门禁Before明确155条失败（植物44、地形111）。全部4605资产/外置包已备份2758929913字节。第一次备份审批超时未执行，短命令重试获批；Windows长路径读取/复制统一加原生长路径前缀，未移到工程外。PID7028原生完成175包保效基线，全部目标无冲突、脏包0。

Task2/3：175目标已全部由原生RenameAssets迁移。地形色图第一次因CDO软引用警示在unattended模式默认Cancel，零目标写入；完整失败journal另存后经已授权的可见确认继续成功。样例批次后错误地强制保存空旧包导致FullyLoad复活旧导出并引用新包私有子对象；真实新目标像素/参数完整，已用原生ConsolidateAssets收敛并清理源重定向，作者改为不保存空旧包。未关闭校验器。57+1项本轮无引用Redirector已原生精确清理；原生公共Owner图155失败转为0。迁移前导出的35张PNG像素仍是严格合同；流送Mip驻留尺寸不作为源图尺寸，UE FName的大小写等价按引擎身份核对，新增两项真实失败回归留证。当前脏包0。
