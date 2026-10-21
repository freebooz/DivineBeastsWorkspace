# SSLM角色技能栏与兔图标接入（2026-10-21）

本轮修正正式技能栏误用美术预览的状态，并将十二生肖各自授权集补为8个可输入槽及独立旧被动。旧被动仍不主动施放，也不在本轮宣称最终被动机制已经实现。

技能栏仍只读取服务器OwnerOnly授权、原生GAS Spec、角色代次和Profile；第一次绑定读取快照，此后通过现有事件更新。点击与数字键统一进入已有InputTag/GAS链，不新增授予入口、播放器或UI轮询。

| 按键 | 现行实际执行 | 身份与限制 |
| --- | --- | --- |
| 1 | 定向飞弹 | 保留原英雄定向飞弹ID、GA和独立冷却 |
| 2 | 火焰弹 | 保留原技能与服务器数值 |
| 3 | 寒冰箭 | 保留原2秒权威起手 |
| 4 | 奥术三连飞弹 | 保留原连发及取消合同 |
| 5 | 治疗恢复 | 保留原治疗ID；恢复有效目标20%最大生命，10秒冷却 |
| 6 | 本角色防御槽临时映射为治疗恢复 | 采用本角色已登记防御技能ID与V5名称；复用原治疗实现及冷却组，尚非最终防御机制 |
| 7 | 本角色控制槽临时映射为寒冰箭 | 采用本角色原技能ID与V5名称；复用冰箭及冷却组，不声称已经实现最终控制 |
| 8 | 本角色特色槽临时映射为奥术飞弹 | 采用本角色原技能ID与V5名称；复用奥术及冷却组，不声称已经实现最终特色机制 |

新增36份Definition与36份独立GA来自真实已保存的同英雄占位资产，新36行数值表保持各自技能ID、等级和数值一致。复用冷却组会使3/7、4/8及5/6互相进入冷却，说明文字明确提示此临时合同。没有修改V6草案审批或把最终机制标为通过。

真实资产制作由Monolith MCP 0.23.0在UE5.8主工程完成。原12份集合及唯一Profile增量修改；新资产位于`/DBAGameplay/Abilities/Placeholders/HotbarFallbacks`。原始字节备份与完整映射见`Saved/Validation/SkillBar20261021/Backup`、`FallbackMappings.json`。回退应先恢复对应集合/Profile与技能栏原包，再按相同发布流程部署，不能仅替换运行中的客户端。

## 兔图标

用户指定下载包`SSLM_Rabbit_V6_24Candidates_64_48_PreReview (1).zip`，SHA-256为`d900078df04b555e809851d8f47f6903df1ef8e6b6afe0652d05f315d77c2307`。

保留包内24份候选源图；按包内预选清单原样导入8份48×48运行Texture2D及8份64×64审核Texture2D。实际1024母版、64图与48图分别验证尺寸和SHA，不把包内旧2048计划路径当成真实尺寸。原始包内中文元数据不用于覆盖正式命名，稳定技能ID及显示名称使用工程V5批准目录核对。

纹理归`DBAHeroPack_Rabbit/UI/Abilities`，启用sRGB、UI无损压缩、NoMipmaps、NeverStream、Bilinear与Clamp。16份UE导出图与源PNG的RGBA像素逐一一致。唯一Profile增量保留14条，八项角色图标及现行1—5占位技能别名已经接通，原V1资源未覆盖。

证据在`Saved/Validation/RabbitV6IconImport20261021`：`Staging.json`、`NativeImport.saved.json`、`NativeSourcePixelComparison.json`及独立重启后的`ColdRead.json`。这些是原生导入/重载与像素合同证据，不能代替真实HUD与人工美术签审。

## 验证状态与运行包

失败基线`SkillBar20261021/RED.before.json`证实正式栏仍为美术预览且全部12角色缺6/7/8槽；保存后的检查`GREEN.saved.json`为12×8通过。新增原生资产合同用例通过12套真实Set、GA、Definition、数值行、Profile和图标的加载校验；表现允许列表先由新增断言复现失败，再补入精确映射。

完整Editor、原生回归、Client/Server、Cook、实际服务/客户端映像、真实准入与人工施法分别记录，不互相替代。后续运行包身份与最终结果读取`Saved/Validation/SkillBar20261021/NativePlan.json`、`ReleasePlan.json`、`ReleaseGate.json`和`RuntimePlan.json`，只认可实际成功结果。

本轮最终结果：Editor完整增量构建通过；8项原生回归全部通过；真实RHI颜色门禁通过；Client/Server完整增量构建与两端Cook/Stage通过。实际Village、MainArena绑定战斗版本`4e8f4179273db70586694f843ed11de23e08a2283ea7ea27c23c2924cbce6f90`。两个新客户端PID为41760、44092，均已核对实际映像/启动身份与引擎就绪日志，未发现启动阶段复制版本错误。它们保留人工登录；真实角色准入、技能按钮/按键1—8与人工美术审核仍须由运行中的客户端完成，未标记人工通过。
