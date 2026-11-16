# SSLM 辰龙V6下载包导入与按角色切换图标（2026-10-19）

用户指定 `SSLM_BEE999V6_ADE58F8AE8BD9BE687_48x48_64x64_16BC%A0.zip`，文件在本机下载目录。16张PNG逐字节匹配同目录高清包及既有冻结来源合同；小包SHA256为 `241495becbfcdc69edeafeef2bdbecdbc342af00aeb465c1d2a8ad4b9402ccbe`。高清包仅补充8张1254母版，不使用既有程序化Review替代。

## 实际导入

现有 `DBAHeroPack_Dragon/SourceArt/UI/SkillIcons/V6/` 新增 `Masters/` 八张1254母版、`Final/48/` 八张运行源、`Final/64/` 八张审核源。原有V1、Review和权威AbilitySet保留。

Monolith在正式UE5.8工程创建并保存 `/DBAHeroPack_Dragon/UI/Abilities/T_DBA_Dragon_S01_CangLongZhua_V6` 至 `T_DBA_Dragon_S08_LongYu_V6` 八张真实48像素Texture2D；磁盘包重载、原生PNG导出与源RGB精确比较均通过。18432像素浮点RHI比较的最大线性RGB误差为0.00374874789446511，门限0.008。sRGB、无损UI压缩、UI组、无Mip、双线性、Clamp与白色乘色合同不做滤色补偿。

唯一 `DA_DBA_Dragon_PlaceholderUIProfile` 保留六项原占位，增补八项V6显示元数据。稳定AbilityId与V5批准名称保持原值；第4/6/7/8项显示三象域、云盾、风压环、风云域。服务器仍未授予V6八技能。

逐图源文件、哈希和对应关系见 [导入清单](DragonV6SelectedTextureImportManifest_20261019.json)。本机完整过程证据在 `Saved/Validation/DragonV6Import20261019/`，包括24PNG核验、118文件导入前保护基线、Profile备份、原生导出、RHI和分阶段接线回执。

## 根据所选角色加载到技能栏

用户后续明确要求图标随所选角色进入技能栏，沿用先前选择的“仅美术预览、不可施放”。当前真实授权的六项占位与V6八项ID不同，不能靠把新图映射给占位火球/治疗来冒充新技能。

实现复用 `UDivineBeastsAbilityBarPanel` 和其唯一 `UDivineBeastsAbilityBarViewModel`。技能栏显式美术模式首次读取 `UDivineBeastsUIClientSubsystem` 的完整选择快照，后续订阅选择、注销事件；按 `SelectedHeroDefinitionId` 查询唯一Profile，通过GamePlatformData两阶段资源租约加载。`ArtPreviewAbilityIds` 保存明确八槽顺序，不按名称或扫描次序推断。换角色先清空旧图并撤销租约；迟到完成、失活和重复绑定受到代次与来源门禁保护。

美术模式八槽均不可激活，命令入口返回 `AbilityArtPreviewReadOnly`，不显示施放快捷键。没有选中角色、资源缺失或英雄尚未配置完整八图时清空；关闭此模式保留既有服务器授权投影路径。UI仍无业务Tick、轮询或新增资产管理器。

## 验收状态

原生纹理、Profile八映射、16PNG小尺寸及5项既有资源/投影/校验回归已通过。后续所选角色接线的完整Editor/Client构建、新增选择事件回归、五英雄八槽顺序和真实技能栏保存/包重载已完成；Client/Server Cook/Stage及双客户端实际启动见 [执行记录](TwoClientValidation_20261019.md)。五英雄图标的实际HUD显示尚未逐项通过人工验收。

首次Standalone只读审查无外服目标；后续双客户端已执行战斗版本一致性门禁并同步两个角色服务器。Client Cook/Stage现已通过，两端已观察到新手村场景。MainArena匹配准入、完整双人玩法和人工美术签收仍待实际验证。
