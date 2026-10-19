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
牛技能唯一UIProfile增补、UE冷回读和原生自动化：已部署脚本及测试源码，具体成功状态以执行日志为准；未有日志时不可声称完成。
正式新手村/5v5 HUD：尚未完成八个V6技能的实际授权和真实对局显示。所有其它生肖与服务器技能资产保持不变。
由于运行器已有多项长时间构建/烘焙作业，相关UE写入和QA工具存在排队情况；不允许为本次操作停止其他会话任务或绕过构建锁。
