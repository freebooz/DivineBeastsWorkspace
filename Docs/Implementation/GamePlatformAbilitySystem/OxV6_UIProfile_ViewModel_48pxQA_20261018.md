# SSLM 丑牛 V6 图标：UI Profile、ViewModel、小尺寸验收

日期：2026-10-18；引擎：UE5.8（虚幻引擎）；英雄：Hero.Zodiac.Ox（丑牛·镇山玄角）。

## 一、设计与版本限制

使用历史优先V6八技能稳定身份：玄角撞（Primary）、裂角（Slot1）、裂地踏（Slot2）、不动玄门（Slot4）、沉步（Slot3）、镇山护壁（Slot5）、顶回（Slot6）、立阵（Slot7）。
V6能力目前没有人工玩法审批，不允许本轮通过UI技能配置授予GAS施法资格。图标为从英雄包原有五张高清美术衍生的临时审核版，不得冒充聊天生成的八张独立原图或3A终稿。

## 二、唯一UI Profile（技能表现配置）

保留 DBAHeroPack_Ox（丑牛英雄内容包）中的 DA_DBA_Ox_PlaceholderUIProfile（唯一Ox主资产），以八条完整 AbilityId（稳定技能逻辑身份）增补仅客户端UI的中文显示名称、V6技能描述和图标软引用；原六条占位技能UI配置必须保留。目标数据资产条目总数为14。
作者工具：Tools/Unreal/UI/BindOxV6ReviewUIProfileUE58.py（可读审查、显式人工执行写入与真实uasset保存）。
一体化执行入口：Tools/Unreal/UI/RunOxV6UIProfileReviewIntegration.ps1（真实UE编辑器写入、独立冷回读、小尺寸QA）。
冷回读：Tools/Unreal/UI/VerifyOxV6ReviewUIProfileUE58.py（对14条主资产数据、八个原生图标和未误授权的六占位集合逐一核验）。

## 三、ViewModel（技能栏视图模型）

保持既有 DivineBeastsAbilityBarViewModel 作为唯一展示数据投影：从实际服务端已授权的Loadout获取AbilityId，经GamePlatformData异步取得技能UIProfile及图标，按FindEntry(Grant.AbilityId)填充Slot.Icon、名称和说明。技能栏部件 DivineBeastsAbilityBarPanel 只渲染已获得的真实授权槽位，不可从美术配置凭空拼装八个可施法按钮。
原生验证测试：Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Tests/DivineBeastsOxV6ArtProfileNativeTests.cpp；要求八项资源各有正确ID、中文名、48像素Texture2D和已授权前空ViewModel，不改变服务端权威。

## 四、64×64 / 48×48 技术与艺术审核

8张64像素PNG位于 SourceArt/UI/SkillIcons/V6/Review64，8张48像素PNG位于 Runtime48；八份游戏Texture2D位于 Content/UI/Abilities/V6/Textures。
源图SHA-256（文件指纹）和像素尺寸逐一校验通过、每尺寸8个文件哈希互异。技术指标：64像素图标亮度标准差约47.5～68.3；48像素约46.3～67.3，明显欠对比或严重爆白风险较低。
但是玄角撞/顶回、裂角/沉步的主体识别较接近；不动玄门/立阵在48像素图标上的差分感知哈希距离仅1/64，表现不出石门与固定盾阵的机制差异。美术批准状态必须继续是 PENDING（待审核），必须独立重绘这些技能后再由人工审定。
技术QA脚本：Tools/Unreal/UI/AuditOxV6SkillIconSmallSizes.ps1，输出牛英雄SourceArt中的 ContactSheet（原尺寸对照图）与 SmallSizeTechnicalReview.json（量化审核结果）。

## 五、当前执行与验收状态

素材导入：八份Texture2D已由UE5.8独立冷回读验证。
牛技能唯一UIProfile原位增补已完成，写入8条V6客户端表现元数据、原6条占位保留，合计14条，已保存原始uasset备份；独立UE5.8冷重载成功，日志位于 `Game/Saved/Validation/OxV6ArtProfileIntegration/UIProfileColdReload.log`，标记 `SSLM_OX_V6_PROFILE_COLD_VERIFY_OK 8 placeholders 6 profileEntries 14 unapprovedGrants 0`。

客户端 `DivineBeastsUIClient`（项目UI客户端模块）增量构建通过，证据 `Saved/Validation/FoundationM0/550ebb58-99ec-433b-a433-8ad0c44ece14/Build`。新建C++编辑器测试 `DivineBeasts.UI.Combat.OxV6.ArtProfileAndViewModel` 首次实际运行失败：`-nullrhi` 下 `Texture2D::GetSizeX/Y`（平台尺寸）返回0；已调整为使用真实导入的 `TextureSource`（纹理源）尺寸并添加显式int32转换，编译对象已通过，但Editor最终DLL链接被外部正在使用的 `UnrealEditor-DivineBeastsUIClient.dll` 文件占用（LNK1104）。不可把编辑器原生自动化标记为通过；运行记录在 `Game/Saved/Validation/OxV6ArtProfileIntegration/NativeAutomation/Report/index.json`，链接问题见 `Saved/Validation/FoundationM0/aefa93ee-24e0-4c9d-8dea-ca03d2855bf4/Build/Editor`。
正式新手村/5v5 HUD：尚未完成八个V6技能的实际授权和真实对局显示。所有其它生肖与服务器技能资产保持不变。
64/48技术检查脚本已在2026-10-19运行，通过8套PNG尺寸、源SHA、亮度与字节去重检查，输出 `SSLM_OX_V6_SMALL_SIZE_TECHNICAL_QA_PASSED 8 64 48 nearPairs=1`；对照图和量化JSON均已生成，唯一明显近似对为「不动玄门/立阵」，差分感知哈希距离1/64，必须重绘后人工优选。未经人工签收不得把技术通过写成美术质量已通过。Editor锁及其他并行作业保留，不强行终止。
