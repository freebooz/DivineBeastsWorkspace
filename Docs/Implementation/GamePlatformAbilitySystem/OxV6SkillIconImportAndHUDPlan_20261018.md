# SSLM 丑牛 V6 八技能图标制作与技能栏接入记录（2026-10-18）

## 目标与真实性边界

本任务仅对 Hero.Zodiac.Ox（丑牛·镇山玄角）的八项V6设计进行**图标和项目UI资源集成**，不创建第二套技能栏，不修改真实GAS授权、不修改技能执行服务器。必须遵守根目录 AGENTS.md（工程规则）。

已核实历史V6审核稿位于 `Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json`，对应 `Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json`。此前会话草案的“角击、崩山顶、厚土甲、震地踏”等不是当前工程V6正式候选，禁止用错误名称覆盖真实AbilityId。

目前V6官方状态为 `PENDING_HUMAN_APPROVAL`（待人工批准）；生产角色仍有6项Placeholder（占位）技能。**制作图标不等于用户批准特殊机制，也不允许修改DefaultAbilitySetId（默认能力集合）伪造授权。**

## 1. 锁定八技能真实设计

| V6顺序 | 槽位标识 | 正确名称 | 正式稳定技能ID | 视觉主体 |
| --- | --- | --- | --- | --- |
| 1 | Primary（主普攻） | 玄角撞 | dba.ability.ox_eight_horn_strike@1 | 玄角近距离撞击 |
| 2 | Slot1（主动一） | 裂角 | dba.ability.ox_eight_horn_break@1 | 牛角破甲、岩纹裂开 |
| 3 | Slot2（主动二） | 裂地踏 | dba.ability.ox_eight_ground_pulse@1 | 牛蹄震地、环形地脉 |
| 4 | Slot4（终极槽） | 不动玄门 | dba.ability.ox_eight_mountain_break@1 | 竖立石门、方向阻挡 |
| 5 | Slot3（移动） | 沉步 | dba.ability.ox_eight_horn_charge@1 | 顶盾稳步推进，并非高速冲刺 |
| 6 | Slot5（防御） | 镇山护壁 | dba.ability.ox_eight_stone_armor@1 | 正面岩盾与反震 |
| 7 | Slot6（控制） | 顶回 | dba.ability.ox_eight_seismic_stun@1 | 牛角反顶，击退正面目标 |
| 8 | Slot7（特色） | 立阵 | dba.ability.ox_eight_steadfast_roar@1 | 固定阵线，队友站后受保护 |

**注意**：V6设计中的特殊墙体碰撞、破甲、稳步推进与击退等机制仍需后续权威GAS实现与核准；图标不证明该技能已有运行实现。Slot4在展示序列第4项、Slot3在第5项属于现有V6数据的稳定槽位设计，不改写或按视觉顺序机械重排槽位。

## 2—4. 候选图制作、人工优选与精修

本会话图像生成已产生构图概念，依据其中的技能主题裁出3张候选裁片×8技能（24张）、另生成8张暂定主图；每张准备256×256客户端源图、512×512放大概念图、64×64与48×48缩小QA纹理，附来源说明与每份文件SHA-256清单。**本轮并没有24张独立1024×1024原创母版**：源自约160px概念图裁片，512px版本是重采样放大；文件命名中Master512只能指保存规格，不表示真实细节分辨率。视觉尚需人工优选与独立高清精修；未经人工认可不得列为正式3A美术资产。

成果在ChatGPT本会话的 `SSLM_Ox_V6_IconPack_Review.zip`（可下载图标审核包）、`OxV6_24Candidates_HumanReview.png`（24候选审核）与`OxV6_Runtime48_HUDReview.png`（48px HUD可读性模拟图）中。工程本地清单为 `Docs/Implementation/GamePlatformAbilitySystem/OxV6SkillIconImportManifest_20261018.json`。

## 5. 导入客户端第三层英雄包

正确目标挂载为 `/DBAHeroPack_Ox/UI/Abilities/V6/Textures/`，物理位置 `Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Ox/Content/UI/Abilities/V6/Textures/`。源图放在本英雄包 `SourceArt/UI/SkillIcons/V6/`，属于第三层客户端表现，禁止硬引用到Dedicated Server（专用服务器）世界地图。

已编写 `Tools/Unreal/UI/ImportOxV6SkillIconsUE58.py`，具有3个模式：

- `inspect`（只读检查）：核实8技能/真实ID/24候选、ZIP来源图像签名/尺寸/SHA、目标目录，缺附件只报等待，不写任何资产。
- `import`（原生导入）：需要正版编辑器进程和显式参数 `-SSLMOxV6IconAuthoring`，只在目标纹理路径尚无资源时，以真实48×48 PNG生成8个48×48原生Texture2D（二维纹理），同时在SourceArt保留256×256编辑母图，保持既有纹理不被覆盖；设置 `TC_EDITOR_ICON`（UI二维未压缩纹理）、`TEXTUREGROUP_UI`（界面纹理组）、`TMGS_NO_MIPMAPS`（不产生Mip层）、sRGB开启、Never Stream（不流送）、双线性过滤；逐一保存并回读。
- `bind`（应用数据映射）：既需 `-SSLMOxV6IconAuthoring -SSLMOxV6UIProfileBind`，也需Manifest中的 `manualReviewStatus=APPROVED`（人工批准）。**仅在现有同一份** `/DBAHeroPack_Ox/UI/Abilities/DA_DBA_Ox_PlaceholderUIProfile` 上追加8个V6展示条目，不修改原有6个Placeholder、AbilitySet或Hero定义，避免两个相同HeroDefinitionId的主资产冲突。

导入还需在运行端出现源附件 `Game/Saved/ArtReview/OxV6_Source_20261018/SSLM_Ox_V6_IconPack_Review.zip`。项目已有的WebCodex会话附件导入工具拒绝当前ImageGen/容器生成附件，错误为 `requires trusted MCP host-file provenance`（需要由宿主提供可信附件来源）；因此**本次未取得原ZIP进入Windows Runner的证据，也未创建本批新Texture2D或绑定UIProfile**。禁止构造伪二进制资源或声称完成。

## 6—7. 复用现有主资产与客户端技能栏

已原生核查Ox UIProfile：`DA_DBA_Ox_PlaceholderUIProfile` 是真实 `UDivineBeastsAbilityUIProfile`（牛技能界面配置主资产），英雄身份 `Hero.Zodiac.Ox`，目前有6个占位技能。不能为同一英雄再创建另一个身份相同的UIProfile。

`UDivineBeastsAbilityBarViewModel`（战斗技能栏视图模型）已从服务器复制的真实 `GrantedAbilitySlot.AbilityId`（授权技能标识）读当前英雄数据，经 `GamePlatformData::AcquireResources`（平台资源租约异步加载）取得唯一UIProfile，收集软纹理路径异步预加载，按 `FindEntry(Grant.AbilityId)` 精确绑定图标，再由既有HUD显示。现有技能栏VM无需硬编码牛技能或新加另一套播放器。**未获授权的V6技能不会伪造或显示可施放状态**。

## 8—9. 小尺寸与实机场景验收

48×48及64×64预览图已离线生成，可对比玄角撞/顶回、不动玄门/立阵的相似轮廓、主体边缘及高光溢出。实际UE纹理尺寸、压缩/颜色、四层质量、客户端Cook、Ox界面技能栏显示与正式新手村/竞技HUD均待源图安全传入后由 `Tools/Unreal/UI/VerifyOxV6SkillIconsUE58.py`（UE原生只读纹理检查脚本）冷回读，再以真实可见客户端截图和人工验收补证。不能将AI生成的合成HUD展示图宣称为UE5.8运行截图。

## 验证状态

- 已检查Ox真实V6技能设计、Ox占位UIProfile真实6项、UIProfile数据结构及AbilityBarViewModel异步加载。
- `Tests/Assets/TestOxV6SkillIconPipeline.py`（稳定身份/槽位/24候选/服务器不授权/主资产唯一性测试）已有5项。
- `Tools/Unreal/UI/ImportOxV6SkillIconsUE58.py` 默认检查应输出 `SSLM_OX_V6_IDENTITY_PREFLIGHT_OK 8 inspect` 及 `SSLM_OX_V6_IMPORT_WAITING_FOR_TRUSTED_ART_UPLOAD`。
- 当前人工选图、真正高清精修、8份原生Texture2D和HUD运行截图仍未完成，因此整体验收结论：**待完成，不能标记正式导入或最终上线**。

## 最短闭环

将本对话提供的ZIP保存到电脑，然后**作为新的用户文件附件重新上传至本聊天**，使HWebCodex获得可验证的可信附件来源。导入器将只读取该附件中的8项精确文件；经编辑器纹理导入、原生冷回读与人工选择批准，再更新已有Ox UIProfile，并在牛的真实技能栏中验证显示。只追加展示数据，不修改服务器V6正式技能批准状态。
