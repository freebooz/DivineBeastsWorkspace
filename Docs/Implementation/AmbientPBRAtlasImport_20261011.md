# 《神兽联盟》六图集 PBR 落叶材质工程导入说明（2026-10-11）

## 一、目标与素材真实性

用户新生成六张自然花瓣、叶片与环境粒子 PBR 展示图，要求在 UE5.8 中直接用于 Niagara（虚幻粒子视觉效果）及未来区域切换。本次资源拆分在 ChatGPT 文件环境执行，得到 **33 类风格、132 张独立 RGBA/法线/粗糙度/不透明度纹理**。已保存原六图集 SHA-256、每张导出纹理哈希和视觉物体尺寸（厘米）提示，并打包为 `DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip`。

需要说明：图中“Normal（法线）”“Roughness（粗糙度）”是生成的展示通道，非扫描测量或高精度几何烘焙；拆分过程的重采样不会真实增加细节精度。上游每格仅约200px，不应声称插值512px等于512px原生细节。

现有共享内容包 `DBAPresentationPack_Core（神兽联盟公共视觉内容包）` 已有33套项目图像/材质/Niagara系统变体/Definition，故按原资源路径增量替换，不创建第二套运行服务。

## 二、插件和三层归属

- 第一层 `GamePlatformVFX（游戏基础平台通用视觉特效插件）`：新增真实 `M_GP_AmbientNature_PBR_Foliage（通用双面植被PBR母材质）`，位于`Content/Environment/AmbientNature/Materials`。双面植被着色模型、颜色、法线、遮罩、粗糙度、非金属镜面和次表面透光输入；不依赖任何项目纹理/项目目录/物种。现有Niagara空中、地面运动、组件池、特效预算和生命周期继续复用。
- 第二层 `MobaCommon（MOBA公共层）`：纯装饰无MOBA专属语义，不新增实现。
- 第三层 `DivineBeasts/DBAPresentationPack_Core（项目公共表现内容包）`：精灵RGBA、Normal、Roughness、Opacity、MaterialInstance等视觉外观放入项目内容包的 `VFX/Environment/AmbientNature（项目环境材质目录）`；区域配置、资源身份和风格映射仍归项目，不让平台层逆向引用。
- `GamePlatformWeather（平台天气插件）`负责风力和雨雪状态；`GamePlatformData（平台数据服务）`负责Definition主资产，`GamePlatformPresentation（平台表现总线）`负责语义解析。Dedicated Server（专用服务器）剥离全部纯表现资源。
- 平台材质内没有强度过高的Unlit Emissive（非受光自发光），防止过去出现的白色过曝花瓣；Niagara旧实例继续引用原Project MaterialInstance路径，更新父材质而无需复制发射器。

## 三、原图资产质量分类

所有导出Maps中每风格具备4张 PNG。
- `auto_candidate（可参与引擎验证的自动候选）`：15类，包括桂花、玫瑰、通用粉/白/红落英、彼岸花、梨花、樱花、梅花、梧桐、柳叶、杨叶、橡叶、白桦、通用黄叶。候选仍需要UE5.8实际光照和表面边缘检查，不等于正式美术签收。
- `review_only（人工必审）`：杏花、玉兰、荷花的Color/Normal来源在展示拼版中被截断，以及红叶/枯叶/松针/棕榈等复杂多面板可能存在通道不完全对齐。
- `fx_reference_only（FX参考）`：灰烬、火星、沙尘、羽毛、纸片、蒲公英、柳絮、萤火虫等含混合粒子，不能把整张组合图当作单一Sprite或误用植被PBR。它们继续复用原有Soft/Glow/漂浮模板，单独制作与验收。

物体尺寸来自常见形态的视觉范围估计，记录在 `Manifest.json` 的 `physicalWidthCm/physicalHeightCm`。如某物种与实际场景尺度不符，须在Niagara变体/项目数据中调整；仅调整材质不会改变粒子Size。

## 四、已执行的真实UE工作

UE5.8编辑器中新增了正式平台母材质：
`/GamePlatformVFX/Environment/AmbientNature/Materials/M_GP_AmbientNature_PBR_Foliage`。
官方Editor保存返回成功，`Saved/Validation/FallingFoliage/PBRAtlas20261011/PlatformPBRMaster.json`记录`created=true,success=true`。这不等于全部实际材质实例已重导。

资源导入工具：
- `PreflightPBRAtlases.py（图集ZIP完整性及安全解包）`验证六图集和132贴图的SHA-256、非法路径和资源身份。
- `ImportPBRAtlasAssetsUE58.py（真实UE材质资源分批导入）`默认`inspect`，只有明确`AMBIENT_PBR_APPLY=approved_apply`才开始替换。先备份旧Texture2D和MaterialInstance的真实二进制，再重导合格候选Albedo/Normal，并新增Opacity/Roughness；材质实例通过同名资源身份重新绑定新平台PBR母材质。Niagara系统本身不被覆盖。
- `RunPBRAtlasImport.ps1（Windows导入操作入口）`提供3批（每批5种）的资产申请、校验和保存，保留日志。

当前ZIP经模型生成通道无法传入HWebCodex Windows Runner（接口提示`requires trusted MCP host-file provenance`），不能宣称原六张图集已导入远程工程。源文件为用户在当前对话生成的素材，不应伪造安全上传来源。用户可以从本对话下载ZIP后复制到下列Incoming目录，导入脚本随即具备完整真实源数据。不得把在工作容器创建的PNG文件误标为Windows工程中已有文件。

## 五、Windows 实际导入操作步骤

1. 从当前对话获取 `DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip`。
2. 保存到 `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature/Incoming/（原图待导入）`。
3. 在UE5.8路径及DivineBeastsWorkspace中执行：
   `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Unreal/VFX/FallingFoliage/RunPBRAtlasImport.ps1`（只核验ZIP和DryRun，不修改资源）。
4. 确认预检无问题后分批运行：
   `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Unreal/VFX/FallingFoliage/RunPBRAtlasImport.ps1 -Apply -Batch 1`，然后Batch2、Batch3。若某次失败，应先审查日志与备份、修复后重新执行，不绕过质量门禁。
5. 回到编辑器，对项目MI的Parent（父材质）、4贴图资源和Niagara Sprite Renderer引用进行只读回读，执行材质/Shader编译，测试角色创建/角色选择、Village（新手村）、OpenWorld（开放世界）不同地区的动态形状、密度、高度、速度、贴地风吹、角色遮挡及性能。

## 六、验收状态

- ✅ 六张展示图在本地被准确标记和拆分；33物种共132个导出PNG；ZIP/Manifest逐项SHA校验通过。
- ✅ UE5.8真实创建并保存平台通用植被PBR母材质。
- ✅ Windows导入和备份脚本已创建；Python与PowerShell语法检查通过。
- ⏳ ZIP原始数据待通过可信方式复制到Windows工程；项目33个MaterialInstance尚未用新六图集替换。
- ⏳ 部分源图轮廓截断/FX多对象未达到直接重导门禁，需提供完整分离精灵或人工重新绘制。
- ⏳ 分批UE真实导入、Material Shader编译、Niagara可见粒子效果/地面不穿插与客户端Cook/Stage/人工验收均尚未取得通过证据。
