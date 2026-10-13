# SSLM PBR 自然飘落材质覆盖验收记录（2026-10-11）

执行工程：DivineBeastsWorkspace（SSLM工作空间）；UE5.8；HWebCodex（远程代码工作区连接器）。

## 真实完成结果

- 原ZIP：DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip；SHA-256 = 07c5b03e056a0318af8c2368cc3a06c39ff2ef26c181b2912ef2f78f57084015。由本机Windows Downloads可信拷贝到SourceArt/AmbientNature/Incoming。官方来源预检33风格/132张PBR纹理/15候选全部通过。
- 三批次：Batch 1 桂花/玫瑰/通用粉色、白色、红色花瓣；Batch 2 彼岸花瓣、梨花、樱花、梅花、梧桐叶；Batch 3 柳叶、杨树叶、橡树叶、白桦叶、通用黄叶。每批真实UE5.8 Editor执行`-Apply -Batch X`，返回5种风格、25项Asset保存、0失败。
- 合计15份`MI_DBA_Ambient_*（项目PBR材质实例）`覆盖，60份PBR Texture2D保存（原Albedo/Normal覆旧及新Opacity/Roughness），所有项目材质实例继承平台通用`M_GP_AmbientNature_PBR_Foliage（平台双面植被PBR母材质）`，不修改平台Niagara底层执行器。
- `UEBatchAllReadbackAudit.json（UE实资产回读证据）`由UE5.8重新加载全部15 MI、60个纹理绑定、30个Niagara SpriteRenderer引用和15份Definition及45份原旧资产备份，结果`passed=true`、0错误。
- `PBRD3D12CompileAudit.json（D3D12编译与运行数据检查）`返回母材质重新编译成功、15份材质实例更新成功，0报错。但Python接口不提供`NiagaraSystem.is_ready_to_run`，NiagaraRuntime就绪情况未完成实测，因此不得将此项误报为所有GPU粒子正式运行通过。
- `PBR15_VisualReview.png（15种深浅背景PBR源图人工审核图）`可供检查图形轮廓、透明边缘和法线/粗糙度；注意这是源贴图合成图，不是UE角色场景实拍截图。
- 此批未覆盖其余18种review_only/fx_reference_only资源，也没有改变现有Peach/Maple/Bamboo/Ginkgo四种历史落叶资源的身份；它们需要独立艺术验收后才能替换。

## 插件与三层架构

1. 第一层GamePlatformVFX（平台通用视觉特效）仅提供共用PBR Materiel与原Niagara运动模块/生命周期/预算控制。
2. 第二层MobaCommon（MOBA共用逻辑）不新增装饰粒子逻辑。
3. 第三层DBAPresentationPack_Core（SSLM公共表现资源）保存15套项目级PBR实例/实际四通道Texture2D/原Niagara变体和Definition；专用服务器排除纯视觉资源不变。

## 后续未通过项

- 15种真实角色选择/创建、Village（新手村）、OpenWorld（开放世界）实际画面与审美验收（花瓣由高空飘落、阴影、碰撞与地形穿插、风吹后起落、实体大小/透明边缘/亮度等）。
- 花瓣物理大小由项目Niagara SpriteSize独立设置；本次仅记录物种cm尺寸，没有声称已完成每种SpriteSize实测。原版本已有花瓣穿地和过曝历史问题，必须继续检验。
- 完整Client Cook/Stage、服务器Paks剥离、真实D3D12游戏运行性能与移动端质量分级验收。
- 18种截断或多对象图暂保留旧贴图，禁止自动通过。
- 如后续其他会话改写同一uasset，在提交前必须做增量Git合并检查。

## 证据路径

`Saved/Validation/FallingFoliage/PBRAtlas20261011/UEBatchImportReport_Batch1.json`、
`UEBatchImportReport_Batch2.json`、
`UEBatchImportReport_Batch3.json`、
`UEBatchAllReadbackAudit.json`、
`PBRD3D12CompileAudit.json`、
`PBR15_VisualReview.png`、
`UEBackup（45份原始二进制备份）`。

本次任务不创建任何新的VFX执行插件，不重新生成工程，也不擅自提交或推送Git。
