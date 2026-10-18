# SSLM 巳蛇 V6 八技能图标：美术、UE5.8 导入及正式技能栏接入实施方案

> 创建日期：2026-10-18。真实完成状态：已创建并核查八张1024×1024独立源稿、八张48×48 PNG及其评审画板；美术仍为程序化初稿，尚未达到电影级3A美术签收质量。未执行UE原生纹理导入、正式技能栏图标替换、GAS八技能授权、Cook或联机运行验证。
>
> 工作空间：DivineBeastsWorkspace（SSLM）；分支main。当前存在其他会话的未提交修改，本次资源及文档独立命名，不覆盖历史V1蛇纹理或并行UI/GAS改动。
>
> 设计依据：Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json（V6历史审核稿）、Docs/Design/ZodiacSkillsV6_1_DetailedProposal_20261018.md（V6.1机制提案）；现行名称以Docs/Design/ApprovedSkillDisplayNames_V5.json为准。色彩唯一权威文件为Docs/Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md。

## 一、八技能与稳定身份

角色为巳蛇·幽鳞玄蛇，定位毒伤法师与路径封锁；独立被动“毒记”不占八个主动技能槽。

1. Primary（普攻）：幽鳞刺，技能ID dba.ability.snake_eight_scale_bite@1。单枚中程蛇鳞针，图标应只突出一枚锋利鳞针与穿刺方向。
2. Slot1（攻击一）：毒牙，技能ID dba.ability.snake_eight_venom_spear@1。单体毒牙命中、有限持续毒伤及条件抑疗；图标突出一对毒牙、毒滴与接触冲击。
3. Slot2（攻击二）：双鳞弹，技能ID dba.ability.snake_eight_serpent_arc@1。两枚清晰分离的交错蛇鳞能量弹，对同目标最多两段；图标不可画成一枚弹。
4. Slot4（终极）：万毒归鳞，技能ID dba.ability.snake_eight_serpent_maelstrom@1。同源毒纹引发减速、抑疗、末段爆发三波毒潮；盘旋蛇身、三层能量浪潮、终极爆发。V6名称待审批，V5现行名称为毒阵爆。
5. Slot3（移动）：蛇行，技能ID dba.ability.snake_eight_sinuous_dash@1。贴地S形合法滑行，经过自身毒径有限加速；突出有朝向的蛇头和运动残像。
6. Slot5（防御）：蜕鳞，技能ID dba.ability.snake_eight_shed_skin@1。蜕去鳞影，清除指定轻度减速并短时减伤；空蛇蜕与脱出的新蛇同时可见。
7. Slot6（控制）：缠纹，技能ID dba.ability.snake_eight_coil_silence@1。踏入蛇纹环先减速，久留才短缠足，不是旧V2无条件沉默；图标突出环状陷阱及足部束缚。V6名称待审批，V5现名蛇纹缚。
8. Slot7（特色）：封途，技能ID dba.ability.snake_eight_venom_mark@1。沿合法地形铺有限毒径并在成熟后引爆，敌人可绕行；图标突出地面毒路与路径封锁，不可与第5槽的角色移动混淆。V6名称待审批，V5现名毒径。

以上编号仅为显示次序；**正式技能栏显示顺序固定为Primary、Slot1、Slot2、Slot4、Slot3、Slot5、Slot6、Slot7**，不能误按Slot编号数字顺序排列。未来修改显示名也不得更改历史稳定AbilityId、输入Tag或现有已发布主资产身份。

## 二、美术与原始资产质量门禁

每技能一张独立方形1024像素或更高的源稿，最后单独精修为48×48 PNG；禁止生成一张八宫格后切片用于生产。真实48像素显示下，主体应占画面70%～85%，前后景不超过三层，优先清晰形状与动作，再表现鳞片、材质和光效。不得把快捷键、文字、冷却蒙版、外边框、施法条、技能名烘到图标中。

巳蛇视觉DNA为玄紫、孔雀青、青玉能量和冷银虹彩鳞片，终极可克制使用暖金；以SSLM全局V3规范为唯一色彩真源，参考明快游戏视觉辨识方法但不能复制任何第三方实际图标。禁止颗粒、噪点、过度像素化、反复锐化、过曝发白、荧光绿、失控的光晕和廉价平面浮雕。要同时检查黑色技能底框与新手村明亮翠绿场景、正常与禁用/冷却蒙版，在真正48像素大小和不同DPI下保持辨识。

目前由Tools/Unreal/UI/BuildSnakeSkillIconsV6.ps1离线生成的16份PNG仅是轮廓明确、可追溯的程序化**视觉初稿**，并非已获批准的电影级3A精修稿。外部AI绘画服务提交失败，未取得可替代的高质量原画。本批初稿特别需要返工：第4项的蛇身/三波浪潮空间层次、第5与第8项的运动主体和地面毒径区别、第6项的空蛇蜕壳层次、第7项的束缚对象可读性。必须先以毒牙、万毒归鳞作为两张独立高清精修样板人工签收，再推广其余六张；保存最终新PNG及重新计算的SHA-256，不能沿用本初稿校验值。

## 三、资源所有权与清单

唯一资源所有者为项目层DBAHeroPack_Snake（蛇英雄内容包），不进入平台GamePlatformUI、不进入MOBA逻辑、不被Dedicated Server硬引用。

源图路径：Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Snake/SourceArt/UI/SkillIcons/V6/Masters/（每张文件名以_Master_1024.png结尾）。

真实48像素PNG路径：同一V6目录下的48/文件夹；所有文件名在SnakeV6SkillIconImportManifest_Draft.json（蛇V6图标待审导入映射清单）中逐一列明。

源图审核画板：同一V6目录的Review/SnakeV6_48px_ReviewBoard.png，此图仅用于审美审核，不能作为图集或正式技能纹理导入。

八份计划中的真实Texture2D归属于DBAHeroPack_Snake的Content/UI/Abilities目录，UE资源挂载前缀为/DBAHeroPack_Snake/UI/Abilities/，逐图命名为T_DBA_Snake_S01_ScalePierce_V6至S08_PoisonTrail_V6等八个唯一资产名。具体完整ObjectPath与稳定AbilityId由上述清单提供。当前**这些V6 Texture2D仍不存在**，不能根据计划路径声称已经导入。

蛇英雄包旧V1五张真实Texture2D、五张原始美术PNG与旧UI Profile保持不变。

## 四、最优实施工序与放行条件

### G0：核对编辑器、权限与并行变更

已经核实main存在其它会话的未提交改动、旧V1资源与已有客户端技能栏代码。前一轮曾观察编辑器未运行；2026-10-18再次检查时已有UE5.8 UnrealEditor.exe指向当前正式.uproject，但其启动参数表明由其它FPS/音频验证会话持有；本HWebCodex会话未获得可用的Monolith UI资产服务连接，不能抢占编辑器或声称能够安全修改生产UI资产。

恢复操作应先找到项目锁定UE5.8引擎并启动正确的Game/DivineBeastsArena.uproject，校验编辑器工程名称与Monolith服务端版本、项目挂载点；如果编辑器依然不可用，保留原生导入任务为阻塞状态，不使用其他方法伪造.uasset或伪称已通过UE验证。

### G1：独立PNG制作与静态测试

**已完成初稿制作**：八张1024母稿、八张真实48像素PNG及独立审核画板。自动门禁应检查PNG格式、尺寸、八个唯一AbilityId、固定槽位顺序、各资源路径与SHA-256；运行Tests/Assets/ValidateSnakeV6SkillIcons.ps1（Snake图标离线验证脚本）并保留输出。

### G2：电影级美术终审

先用独立1024大画幅AI生成或手绘艺术加工完成毒牙与万毒归鳞两枚样板。按青玉材质、电影级层次、角色特征和实际48尺寸返工，审核通过后制作六枚同体系图标。通过固定DPI的技能框、森林地形背景及真实48px截图人工检查。未通过的人工作品不得通过G3进入正式游戏素材目录。

### G3：Monolith真实Texture2D创建（当前阻塞）

UE编辑器与Monolith连接恢复后，使用其真实UI工具（如ui.import_texture_from_bytes）逐张导入48×48 PNG生成八份独立Texture2D，禁止创建文本或更改扩展名冒充资产；再用引擎属性操作回读，确认TextureGroup=UI（用户界面纹理组）、Compression=UserInterface2D（UI二维压缩）、sRGB=true、MipGenSettings=NoMipmaps（不生成Mip）、NeverStream=true、Filter=Bilinear（双线性）、AddressX/Y=Clamp（边缘钳制）；SourceSize必须为48×48。

每份资源保存后须独立冷重载，核验路径唯一、图像源HASH与真实纹理设置，写入蛇英雄包原有MonolithGenerationManifest.json及项目约定的Monolith生成证据清单。SSLM禁止使用已知额外Gamma的Monolith widget截图模式做配色证据，使用真实Client或RHI单次编码回读；画刷乘色保持白色，不因错误截图反向修正颜色。

### G4：独立视觉审核，不提前解锁GAS

通过现有DemoHub/GamePlatformDeveloperTools工具审核八张已导入真实Texture2D：显示真正48px尺寸、V5已批准名与V6提案名的对照、正常/灰显/冷却状态和翠绿地形对比。人工批准前不得改唯一蛇UI Profile，临时预览只能做美术审查，不产生服务器技能授予。

### G5：在技能正式批准后接入原有八槽技能栏

项目已有完整调用链：Hero.Zodiac.Snake（角色身份） → DivineBeastsAbilityUIProfile:Hero.Zodiac.Snake（唯一UI Profile主资产） → GamePlatformData世界租约异步加载Profile与8个图标 → UDivineBeastsAbilityBarViewModel读取服务器OwnerOnly的已授予AbilityId及GAS Spec → Profile.FindEntry(AbilityId) 精确匹配 → FGamePlatformUISlotState.Icon → UDivineBeastsAbilityBarPanel中真实UImage.SetBrushFromTexture显示。

现有UDivineBeastsAbilityUIProfile::ValidateProfile已经拒绝重复技能ID、空中文显示名/说明与空图标；已有面板支持八个槽位且事件驱动，无需重复建立HUD或新平台逻辑。正式切换前，由玩法团队完成V6能力Definition、GA、GE、AbilitySet、权威伤害/区域/位移/控制、必要VFX与数据审批，确认真正可由服务器Grant。然后经Monolith编辑已存在的唯一DA_DBA_Snake_PlaceholderUIProfile.uasset：准确写入8个稳定技能ID、已批准中文名称与说明、八个Texture2D软引用。**V6未批准的3项名称继续展示V5名称，不允许擅自替换为待审名**。禁止创建与现有Profile相同PrimaryAssetId的第二份生产资产。

视图模型仅表现已获授权的技能，不因图标出现自授技能；异步回调需核对英雄与角色代次，换英雄、换Pawn、世界切换与销毁释放自身租约。UI不使用常驻Tick查询，不在客户端计算技能权威命中/伤害。

### G6：真实UE5.8与多人验收

按既有锁定参数优先进行增量编译与实际编辑器资产有效性校验，不为仅图片内容默认Clean。新手村进入蛇角色时实测八项已授权技能→正确槽位→真实Texture2D加载→冷却/禁用/Tooltip/按键，角色死亡、沉默、换角色、缺失资产与旧回调不得出现旧图标或错误可释放状态；双客户端/多人同蛇测试OwnerOnly授权及GAS时序。

技能栏保持真实图标48×48、总高不超过65逻辑像素、顶部3像素施法条无遮挡与底边平齐；绿色生命条、下面蓝色能量条、独立暖金气势正确显示。实机1080p、1440p、4K及缩放DPI固定曝光截图对照颜色；只将必要UI Texture打入Client Cook，Dedicated Server Cook不能携带纯客户端纹理。记录每条验证的真实命令、截图、日志、Cook产物和人工审核结果，不以代码或计划文件替代成功证据。

### G7：版本控制与回退

仅提交本次蛇V6资源、检查脚本、清单、方案以及之后确实通过UE验证的原生资产与Profile变更。在多人共用dirty main时禁止无差别暂存全部文件或覆盖他人RootLayout、GA蓝图、配置及AGENTS。正式合并先协调对应未提交资产持有人并完成门禁；失败时首先恢复蛇唯一生产UI Profile的旧图标软引用及原有能力集，不删除旧V1资源，不能用图标资源修改代替已授予技能的权威实现。

## 五、阶段性结论

当前能证明的是G1程序化PNG图源、独立文件尺寸与哈希；尚需G2电影级美术审查、G3 Monolith原生资产导入、G4审美签收、G5真实八技能授权/唯一Profile绑定、G6真实客户端及服务器验收。任何“八技能已加载到正式技能栏”或“达到3A图标质量”的结论现在都没有引擎或美术证据支撑。

## 六、2026-10-18 用户确认九阶段工序后的最新状态

用户明确锁定的工序为：①冻结8技能定义→②每技能3张候选图→③人工优选→④精修8张正式图标→⑤导入蛇英雄内容包→⑥建立/更新Ability UI Profile→⑦接入现有ViewModel→⑧64×64/48×48小尺寸验收→⑨新手村/战斗HUD真实游戏验收。

本次新增正式可检索的视觉设计冻结单：Docs/Design/SnakeV6SkillIconVisualFreeze_20261018.json（巳蛇V6八技能图标视觉设计冻结单），已用项目源V6 JSON实际核对8/8项技能ID、V6设计名和输入槽位。此冻结仅限**技能图标设计及其稳定身份**，不代表V6尚未批准的复杂玩法数值、技能授予或待审名称已批准。

已通过原生图像生成形成8行×3列候选概念画板，并导出24份独立低分辨率候选预览、24份64×64和24份48×48审核图、八份单技能A/B/C预览和两页人工审核画板。候选作为本次ChatGPT会话附件提供，压缩包名为SSLM_SnakeV6_24_Candidates_Preview.zip，SHA-256为23797ea9280b0bfb7dcb7334e567a938fb929b364cdfdad0468de7f2cbc209c7。**压缩包尚未导入远端工程**；该图像生成工具产物不是可信用户上传附件，HWebCodex的import_conversation_files_to_project拒绝直接引入。不可据本计划认定24份预览已存在于工作空间。

24份独立候选图来自概念画板的约72px单格，文件仅作为造型/构图优选而非高清母稿；真实64/48审核可以检查轮廓，但无法证明未来1024母稿美术质量。当前正式变更状态：第1项完成（视觉冻结），第2项完成（会话外部低分辨率候选供选择），第3项**等待用户为八项分别选A/B/C或明确退回**，第4—9项**不得提前进行生产资源替换**。人工选择示例：1A 2C 3B 4A 5C 6B 7A 8C（仅格式示例，非默认已选方案）。

后续获得明确选择后：严格根据被选中的8种构图逐张绘制独立≥1024源稿，通过美术人工审核后制作64及48正式PNG，校验源图/真小尺寸HASH与颜色；实际Monolith编辑器连接稳定后导入8份UE Texture2D，再更新既有唯一DivineBeastsAbilityUIProfile:Hero.Zodiac.Snake，不新建重名主资产、不覆盖并行会话蓝图；由现有UDivineBeastsAbilityBarViewModel/UDivineBeastsAbilityBarPanel承接表现，只有GAS服务器权威已批准并授予的技能才显示可用。最后分别核对新手村和多人竞技HUD、Client Cook及Dedicated Server资源剥离，提交真实引擎运行证据。

本轮没有改写8项正式技能定义、V5已审批名称、蛇现有UI Profile、GAS技能蓝图、旧V1图标或正式地图；没有导入新UE Texture2D，也没有声称完成第5—9项。所有候选供人工审核，明确以人工选择为后续制作的先决门禁。

