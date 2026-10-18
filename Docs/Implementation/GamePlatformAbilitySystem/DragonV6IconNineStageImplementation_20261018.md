# SSLM 辰龙V6八技能图标九阶段实际实施与验收 V1

日期：2026-10-18。工程：DivineBeastsWorkspace，UE5.8（虚幻引擎），英雄Hero.Zodiac.Dragon（辰龙·御霄苍龙）。本文件属于图标制作执行记录，不能用来证明正式UE资产或GAS已变更。

权威来源：
- Docs/Design/ZodiacSkillsV6_1_DetailedProposal_20261018.json（V6.1技能审核提案）
- Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json（V6历史招式审核稿）
- Docs/Art/SSLMColorRegistry_V3.json（全局配色真源）
- AGENTS.md（三层插件架构、UI资源制作规范及服务器隔离）

## 1. 冻结八项设计供图标创作

本批锁定八项AbilityId（唯一技能编号）、八槽身份、动作及图像语义，但不越权批准玩法数值或服务器GA/GAS授予。V6.1提案尚在审核，V5正式名字必须继续作为现行生产显示。

| 画面序号 | V6图标主题 | V5正式显示名 | 输入槽位 | AbilityId（稳定技能编号） | 视觉轮廓 |
|---|---|---|---|---|---|
| 01 | 苍龙爪 | 苍龙爪 | Primary（普攻） | dba.ability.dragon_eight_dragon_claw@1 | 三道苍青龙爪 |
| 02 | 雷裁 | 雷裁 | Slot1（攻击一） | dba.ability.dragon_eight_cloud_scale@1 | 落雷柱与落点 |
| 03 | 雷息 | 雷息 | Slot2（攻击二） | dba.ability.dragon_eight_azure_breath@1 | 龙口喷出扇形云雷 |
| 04 | 苍龙定天 | 三象域 | Slot4（终极） | dba.ability.dragon_eight_dragon_ascension@1 | 神龙环绕风云雷三阶段爆发 |
| 05 | 御风 | 御风 | Slot3（移动） | dba.ability.dragon_eight_cloud_stride@1 | 龙形风影与突进方向 |
| 06 | 云护 | 云盾 | Slot5（防御） | dba.ability.dragon_eight_dragon_scale_guard@1 | 龙鳞云盾 |
| 07 | 龙压 | 风压环 | Slot6（控制） | dba.ability.dragon_eight_thunder_cloud_lock@1 | 外推内牵双风环 |
| 08 | 龙域 | 风云域 | Slot7（特色） | dba.ability.dragon_eight_cloud_blessing@1 | 有界风云雷领域穹顶 |

独立被动天象轮转不占八个主动槽；不新增第九个可释放按钮，也不恢复已删除的王印、五行克制或跨英雄关系玩法。

## 2. 统一图标美术质量

参考经典MOBA技能图标高辨识度、明亮魔法氛围，不复制任何现有商业游戏图标。神兽联盟的角色视觉必须第一眼苍龙，第二眼看懂爪击、落雷、吐息、终极、移动、防御、控制、领域的差别。

严格使用现有V3配色：玄墨青#10282D、深碧玉#227E70、灵玉青#68D7B4、银青#B5CADC、寒渊蓝#409CE5、玉脂白#E8F0E7、鎏金#D9B773。主体至少占画幅65%～82%，前中后最多三层。禁止图标上绘制技能框、快捷键、文字、冷却扇形、过度白光、粒子噪点与颗粒锐化。

所有正式图标必须分别原生创作1024×1024母版，不允许将总览图中的小图裁切放大后当作独立母版。独立母版后期高质量缩小制作256、128、64和48四尺寸，48必须实际进行视觉修边。

## 3. 24张候选及24条独立AI绘图指令

实际已输出：24张独立1024程序化构图参考图、24张64样本和24张48样本，共72张PNG；源/小图SHA-256校验通过，八个技能各有A/B/C三个不同构图。它们是草图审查资源，并非电影级3A正式AI母版。

目录：Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon/SourceArt/UI/SkillIcons/V6/Review（龙V6构图评审目录）。
- Candidates1024：独立构图母图24张。
- Previews64：64×64真实缩图24张。
- Previews48：48×48真实缩图24张。
- DragonV6_24CandidateContactSheet.png：A/B/C三行八列人工审核画板。
- DragonV6CandidateReviewManifest.json：源文件和缩图SHA、大小、AbilityId及PENDING状态。
- DragonV6_24IndependentAIArtPrompts.json：24份独立高质量AI绘图指令和负面提示词。
- DragonV6_HumanReviewSelections_Draft.json：8个选择全部choiceABC=null，正式获批0项。

必须由用户选定8个A/B/C。任何自动评分或程序化草图完成不代表审美签收。已连接的第三方图像批量服务本轮因免费账户要求basic等级而拒绝提交：没有24张高品质AI完成品，不能虚报。

## 4. 精修正式8张（待人工优选）

人审按序号回复1A 2B 3C等八项选择，确认后用对应的独立提示词、经授权的图像创作手段重新绘制8个1024真正高质母版。检查纹理、刻画层次、主体、光照、颜色与48px可读性；然后生成四个实际分辨率版本，保存母版和所有输出的Hash。禁止靠程序化预览图或128小图上采样冒充完成精修。

## 5. UE5.8真正英雄内容包导入

必须保留DBAHeroPack_Dragon（辰龙资源包）身份和旧5张V1真实Texture2D资产。正式新48资源母版属于同包SourceArt/UI/SkillIcons/V6/Masters，正式UE资产属于同包Content/UI/Abilities/，命名采用T_DBA_Dragon_S01_CangLongZhua_V6至S08_LongYu_V6。

必须用工程批准的Monolith MCP（虚幻编辑器自动化）创建或修改原生UE UI资产。正式属性应为TextureGroup=UI（界面组）、sRGB=true、按项目当前UI贴图规范采用UI/无损压缩、NoMipmaps（不生成Mip映射）、NeverStream（不流送）、Linear（线性过滤）与Clamp（边缘钳制），保存后由新UE Editor独立冷重载并核对SizeX/SizeY真实48×48、源路径与Hash。不能将PNG改名成.uasset冒充。当前WebCodex的Monolith服务列表为空，因此此阶段尚无真实导入证据。

## 6. 唯一Ability UI Profile（界面配置）保持稳定

现行Dragon已有DA_DBA_Dragon_PlaceholderUIProfile.uasset（龙旧表现主资产），还包含5张旧V1图标；不能新建第二份主资产身份DivineBeastsAbilityUIProfile:Hero.Zodiac.Dragon，因为UAssetManager会出现重复歧义。

项目客户端UDivineBeastsAbilityUIProfile真实字段是HeroDefinitionId（英雄身份）、Entries（技能图文条目）；每项FDivineBeastsAbilityUIEntry包含AbilityId（ID）、DisplayName（显示名）、Description（说明）、Icon（TSoftObjectPtr<Texture2D>软纹理）。需先由V6正式能力团队完成GAS Definition/GA/GE/AbilitySet和OwnerOnly实际授权，随后人工签字后原位更新唯一UIProfile的8条，不允许自行把服务器未授权技能标为可释放。V6审核名与V5生产名冲突的第04/06/07/08项在正式审批之前只用于开发审核预览。

## 7. ViewModel与真正战斗HUD链路

已存在的真实链路：服务器GAS/Loadout授权 → OwnerOnly客户端槽位快照 → UDivineBeastsAbilityBarViewModel（技能栏视图模型）通过GamePlatformData世界租约软加载Dragon UIProfile和8个Icon → 按稳定AbilityId映射FGamePlatformUISlotState.Icon → UDivineBeastsAbilityBarPanel（技能栏面板）用AbilityIcon.SetBrushFromTexture刷新已驻留Texture2D。槽4是终极Slot4，槽5是位移Slot3，不能将Slot1至Slot7按数字排序。

旧被动仍可能在旧五槽Widget中显示；已审核技能栏蓝图历史状态是5个真正可交互子控件加3个空背景，不能因为C++已经具备八项绑定就谎称WBP八槽装配完成。必须使用Monolith核对现有真实Widget树、在不改用户已审核几何的前提下把后三格变为真实可交互Slot，验证仅已获权威授权槽可点击、旧异步回调无法复活角色图标、无热键和无授权时保持安全空槽。

## 8. 64×64与48×48缩图验收

已完成72张程序化概念图的尺寸和SHA完整性、基本亮度/色彩变化技术检查；终版尚需真实美术审核。每张作品须在暗色战斗场景和明亮峡谷背景中按实际48px检查主体与动作、边缘、对比度、技能间识别差异，避免误判缩略图的4倍放大版。图片RGB与真实RHI结果颜色一致性、冷却蒙版、灰显、输入按键等由现有UI控件提供，图标本身不烘。

## 9. 正式Village及Combat HUD实际验证

运行Game Client进入Village（新手村）与竞技场，真正查看Dragon 8个已授予技能的图标/Tooltip/冷却/气势消耗和技能释放；验证1080p/1440p/4K、PC与移动DPI、切英雄、复活、异步加载晚到回调、不同网络延迟。检查Client Cook确实含全部8真Texture2D及主资产，Server Cook不得包含纯UI纹理硬依赖。人工审美批准后，备份原5V1/Profile原生SHA并仅切换已批准的映射；保留原纹理和回退机制。

## 当前阶段真实结果

- 步骤1：八技能用于美术的动作/ID/槽冻结完成；现行生产名保持V5未覆盖。
- 步骤2：24张独立程序化图标构图草案已生成，24份独立正式AI绘画提示词已生成；不等于24张3A AI美术终稿。
- 步骤3：人工优选尚未完成，8项选择均PENDING。
- 步骤4：没有正式8张电影级精修图标，不能放行。
- 步骤5—7：未获审批且没有Monolith可用连接，原生Texture2D、Profile、AbilitySet及Widget未修改。
- 步骤8：候选64/48技术门禁已通过；终版小尺寸美术审核未进行。
- 步骤9：无本批龙V6八图正式Village／HUD运行证据，未宣称已完成。

结论：当前已取得可供用户逐项选型的24份独立图像和全套24条AI精修提示词，等待用户按八项回复A/B/C后，才能推进正式艺术重绘与UE绑定。
