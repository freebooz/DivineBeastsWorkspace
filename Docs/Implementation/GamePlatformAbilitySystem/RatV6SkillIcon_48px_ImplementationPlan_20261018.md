# SSLM 子鼠 V6 八技能 48×48 图标制作、导入与技能栏接入实施方案

> 日期：2026-10-18。性质：已审查源码后形成的实施计划，不是美术资源生成、UE导入或HUD验收完成证据。
> 工作空间：DivineBeastsWorkspace；引擎：锁定UE5.8；当前V6为历史技能设计审核稿，V5仍是已批准名称真源。
> 直接规范：AGENTS.md、Docs/Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md、Docs/Implementation/UITextureVisualPolicy.md。

## 一、八槽稳定对应关系

| 序号 | 输入身份（客户端只读） | 完整AbilityId（稳定技能编号） | V6设计名／当前V5名 | 玩法与图标必须辨识的动作 | 新图标资产名 |
|---|---|---|---|---|---|
| 1 | Primary（普攻） | dba.ability.rat_eight_fang_cut@1 | 影牙斩／影牙斩 | 横扫单击；圆耳双齿鼠面加一道银紫牙刃 | T_DBA_Rat_S01_FangCut_V6 |
| 2 | Slot1（攻击一） | dba.ability.rat_eight_crescent_blade@1 | 咬影／伏牙刺 | 低伏扑咬；背侧附伤，聚焦一对亮牙与伏击黑影 | T_DBA_Rat_S02_ShadowBite_V6 |
| 3 | Slot2（攻击二） | dba.ability.rat_eight_cross_fangs@1 | 交牙斩／交牙斩 | 最多二段交叉切割；两道X形银紫牙刃 | T_DBA_Rat_S03_CrossFangs_V6 |
| 4 | Slot4（终极） | dba.ability.rat_eight_shadow_hunt@1 | 万鼠影袭／三影袭 | 最多三段合法换位；三道有明显次序的鼠影与单个落点 | T_DBA_Rat_S04_ShadowAssault_V6 |
| 5 | Slot3（移动） | dba.ability.rat_eight_shadow_step@1 | 钻缝／穿隙 | 仅沿合法墙边草隙通过；鼠身钻入发亮石缝 | T_DBA_Rat_S05_CrackSlip_V6 |
| 6 | Slot5（防御） | dba.ability.rat_eight_mist_guard@1 | 藏尾／敛影 | 短避选/移速并留幻尾；长鼠尾成旋涡而主体退隐 | T_DBA_Rat_S06_TailVeil_V6 |
| 7 | Slot6（控制） | dba.ability.rat_eight_shadow_snare@1 | 咬筋／绊步牙 | 脚踝咬击减速，有限抑制位移；牙咬踝部及限制环 | T_DBA_Rat_S07_TendonBite_V6 |
| 8 | Slot7（特色侦察） | dba.ability.rat_eight_fang_mark@1 | 嗅踪／嗅踪 | 有界方向感知，非全图透视；鼠鼻/胡须/玉青嗅迹/方位符号 | T_DBA_Rat_S08_ScentTrace_V6 |

独立被动嗅弱不占八槽；后续单独设计第九枚图标。五项V6拟议改名须另行批准，不直接覆盖V5现行显示名。

## 二、美术质量硬指标

1. 一技能一图，禁止把8个小图放进同一画面后切出正式纹理。先为每项分别创作独立1024×1024（或更大）的高质量源稿，再人工调整构图、主体、光照、颜色与留白。
2. 王者荣耀作为图标视觉辨识度和鲜明魔法氛围的参考，不复制其实际图标。SSLM按玄墨紫、月光银和少量碧玉青创作；终极可点亮少量暖金。所有颜色以现有V3颜色规范为唯一项目真源，不另造竞争色板。
3. 明确是鼠而不是狼：圆耳、小鼻、外露门齿、细胡须、细长尾；技能3可用牙刃作为独立形体，技能7可用咬踝部而非完整鼠头，技能8必须以侦查方向标为主。8枚轮廓明显不同。
4. 单格48×48真实显示仍能辨认动作；主体占画面主要区域，前后景最多三层。严格控制碎石、粒子、亮斑与锐化，禁胶片颗粒、噪点、锯齿、过强泛光、霓虹白边和连续细纹。
5. 技能栏框、快捷键、冷却蒙板、施法条均是当前UI控件动态元素，不烘在图标PNG。背景可是不透明的深色方形技能画面，或经Alpha边缘检查的RGBA；不得有白边、棋盘底图、文字、水印或海报。
6. 1024原稿必须保留。单图经过色彩正确的高品质缩小和48像素级修饰后，分别导出真正宽高都是48的PNG；额外提供以最近邻放大4倍的检查图供视觉复核，但它不是导入资产。必须在技能栏真48显示尺寸核验，禁止靠无节制锐化补偿缩小损失。
7. 每张最终输出附FileName、48×48尺寸、sRGB/RGBA模式、源稿与衍生PNG SHA-256、人工审核状态；8张纹理必须为8个独立文件，不能重复哈希或名字。

## 三、真实工程源与目标目录

- 1024源稿：Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rat/SourceArt/UI/SkillIcons/V6/Masters/ 下，每个资产名后加 _Master_1024.png。
- 真正48纹理PNG：同包 SourceArt/UI/SkillIcons/V6/48/ 下，名称为第一章最后一列加 .png。
- 八份真实UE Texture2D：同包 Content/UI/Abilities/ 下，以第一章最后一列加 .uasset 保存。
- 运行软资源路径：/DBAHeroPack_Rat/UI/Abilities/资产名.资产名。
- 所有权：项目层DBAHeroPack_Rat（子鼠英雄资源包），而非通用GamePlatformUI或服务器技能Definition目录；不得借此把UI Texture硬引用给专服。
- 当前Rat已有5枚旧V1原生图标及5张1254像素源PNG；禁止覆盖。当前旧DA_DBA_Rat_PlaceholderUIProfile.uasset仍有其他未提交改动，必须经实际变更协调确认后才允许修改。

## 四、质量优先的逐阶段实施

### G0｜冻结现状与审查权限

确认当前UE5.8编辑器确实打开Game/DivineBeastsArena.uproject，检查Monolith MCP版本及项目根目录，记下本次Rat 5图标与当前UI Profile资产指纹。核对V6八AbilityId、现行V5名称和8槽输入Tag。确定当前持有Rat UI Profile未提交变更的并行任务，未经协调不得覆盖/重命名/另建重复主资产。

先解决配置资产发现前置：当前Game/Config/DefaultGame.ini存在合并冲突标记，应由配置所有者处理；UE AssetManager能否查询出DivineBeastsAbilityUIProfile:Hero.Zodiac.Rat须做真实UE只读查询。仅在缺少真正扫描能力时，按Client/Editor范围有条件追加主资产类型及Rat目录的精确注册，不向服务器扫描客户端纹理。

### G1｜两枚样板先通过，再制作其余六枚

先制作视觉差异最大的咬影（近距单目标咬击）与万鼠影袭（三道明确阶段鼠影）：每项独立生成1024原稿，人工核对48显示辨识度、鼠形、同一角色颜色、原版V6机制与终极强度区别。未通过则只返工样板，不批量生成96个生肖技能图标。

样板通过后，制作影牙斩、交牙斩、钻缝、藏尾、咬筋和嗅踪；统一边缘光宽度、暗部、留白与饱和度，重点避免八张都是紫黑动物头。保存Master、48 PNG与源图哈希。

### G2｜逐一对48像素进行专业精修

每张真实48×48 PNG通过自动门禁：可识别PNG，宽/高均48，RGB/RGBA且sRGB，八份文件名与SHA-256不重复，不存在全透明或明显Alpha误合成。使用高质量缩小过滤和必要的人工48px清边，不做破坏质感的反复缩放、加颗粒、过锐化或2幂强制补边。

评审画板按真正48px显示全部八图，分别放在当前青玉技能框、深色战斗场景和明亮森林地形中检查；对背景不同造成的视觉差异做艺术修正，不允许改变已审核旧技能栏几何。

### G3｜Monolith中生成真UE纹理

本项目AGENTS要求：所有SSLM UI原生资产创建/修改由Monolith MCP完成，先确认Editor进程、工程和插件挂载点。调用ui.import_texture_from_bytes（通过字节导入）创建8个真正Texture2D，再用editor.run_python（编辑器属性操作）为每张回读并核实：
- Texture Group = UI（界面纹理组）；
- Compression Settings = UserInterface2D（UI二维图像，优先避免有损块效应）；
- sRGB=true、Mip Gen Settings=NoMipmaps（无Mip链）、Never Stream=true；
- Filter采用Bilinear（双线性）并在真实48显示验证；X/Y边缘为Clamp（边缘钳制）；
- 调色属性归零或保持白色，不进行第二次Gamma、亮度、饱和度/锐化修正，Widget画刷乘色为白；
- 报告应有每个Texture2D真实SourceSize=48×48、压缩/颜色/资源组、主资源路径、源与原生资产哈希及保存、独立重新打开证据。

现有Tests/Assets/AuditUIColorImportedAssetsMonolith.py默认只允许DBAUIPack_Core/UI路径，不能用原版结果冒充Rat包审查通过；复用其颜色导入检查逻辑，在本次专用门禁显式白名单DBAHeroPack_Rat/UI/Abilities八资源。禁止使用当前存在二次Gamma问题的Monolith capture_scene_preview(widget)验收颜色；使用真实客户端截图或DivineBeasts.UI.Color.SourceToSlate的RHI回读。

### G4｜先建设真实人工图标审核入口（不授予未经批准技能）

现有DemoHub（插件演示中心）使用开发模式展示八个真正已导入的Texture2D，支持选择单技能、V5/V6名称对照、真实48显示、深浅背景切换、正常/灰显/冷却示意、退回和审核拒绝。使用既有GamePlatformDeveloperTools的ReviewCase/Report（人工审核），由真实审核人确认图标清晰、图形唯一、配色统一及生肖血统，保持Pending（待审）直至真实签收。

审核入口允许只读预览“未来V6八图”，**不得凭预览伪造服务器GAS授予**。当前正式主资产稳定身份是DivineBeastsAbilityUIProfile:Hero.Zodiac.Rat；不要同时让新旧两份具有该主身份的UI Profile进入AssetManager扫描，否则查询会有重复歧义。新草案可以保存为审查JSON/瞬态UI状态，正式映射只有在G5后切换。

### G5｜正式八技能获批后，接入唯一Rat UI Profile

先由能力系统团队将8项实际技能Definition/GA/GE/AbilitySet完成权威实现和授权、客户端OwnerOnly技能投影/原生AbilitySpec与AvatarGeneration确认；此动作**不属于制作图标即可越权完成的事项**。
项目现有 ViewModel 使用 UAssetManager::GetPrimaryAssetPath（通过稳定主资产身份取得软路径）查找 DivineBeastsAbilityUIProfile:Hero.Zodiac.Rat；必须实际确认返回路径有效，才可以申请Profile和图标的两份 Data 世界租约。

之后由Monolith编辑器更新唯一正式Rat UI Profile的8个FDivineBeastsAbilityUIEntry条目：AbilityId用第一章稳定ID精确相等，DisplayName读V5现名（V6更名另经批准才替换），Description是真实经批准的技能说明，Icon为该条Texture2D软引用。UDivineBeastsAbilityUIProfile.ValidateProfile要求所有条目具有唯一稳定ID、非空中文显示与描述、非空图标软引用。

现有真实绑定路径：
HeroDefinitionId=Hero.Zodiac.Rat → GamePlatformData（数据插件）加载主资产Profile → 批量异步申请8张图标世界租约 → UDivineBeastsAbilityBarViewModel从OwnerOnly服务器Grant/原生GAS Spec提取当前已授予AbilityId → Profile.FindEntry(Grant.AbilityId) → FGamePlatformUISlotState.Icon → UDivineBeastsAbilityBarPanel的AbilityIcon.SetBrushFromTexture。

当前八个屏幕位置按Primary、Slot1、Slot2、Slot4、Slot3、Slot5、Slot6、Slot7绑定；第4格是终极Slot4，第5格是移动Slot3，不能简单按Slot3=第三显示格排序。固有被动嗅弱不占这八个主动槽，旧五槽被动兼容显示与新八槽分离处理。

只显示已获授权的8项技能；图标加载失败保持空/中性图标，不把数据资产中的美术条目作为本地可释放权威。原生加载回调需核对弱引用、HeroId、LoadGeneration及当前Profile/IconLease，换英雄、换Pawn、销毁、旅行时释放本ViewModel自己的两份世界资源租约。

### G6｜运行与性能验收

UE5.8对改动的真实目标做增量构建：项目技能UI Client、Rat内容包装配与必要的游戏GAS依赖；Monolith Widget创建/保存/冷重载，正确绑定八个AbilityIcon命名子控件、实际输入按键和焦点。技能条保持用户已定约束：图标48×48、底部齐平、总高不超过65个逻辑像素、顶部3像素施法条不可覆盖、生命绿、能量蓝与独立暖金气势显示正常。

真Game Client进入Village：检查8个权威授予、即时切换Rat与其它英雄、死亡/沉默/眩晕、冷却、角色代次切换、资源缺失、旧加载回调、多人同选Rat、客户端不同DPI及1080p/1440p/4K实机截图。验证源PNG与屏幕RHI颜色一致，禁用时蒙版正确不破坏主体纹理。

最终要做真实Client与Dedicated Server独立Cook/Pak清单对比；Rat内容包中的纯UI Texture必须在Client可发现/可加载，Server不应携带该美术内容或产生硬引用。Profile资产每次切换保留版本与回滚证据。全部人工审核未完成时禁止宣称“已装配并可上线”。

## 五、实施交付清单及回滚

完成的可验收结果必须有8份独立48×48 PNG、8份1024源稿、8份真实UE Texture2D、唯一Rat Profile八项稳定ID映射、真正八技能权威授予、现有真实技能栏Widget截图及冷重载/多人Cook证据、编辑器人工审核结果。不得以一张8格预览图或文档代替。

任何环节失败：不触碰旧V1五图，不直接删除旧Placeholder UI Profile；G3或G4失败仅撤销本次新纹理和预览引用；G5或G6失败先恢复此前已签名的唯一Rat Profile旧Entries和真实默认AbilitySetId（只有实际发生修改时），保持其它英雄资源不变、保留原始日志与SHA-256。
