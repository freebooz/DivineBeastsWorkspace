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
## 七、2026-10-11 18:38 用户重新上传后的传输核查

用户已在同一会话上传原始ZIP `DivineBeasts_NiagaraPBR_AtlasAssets_20261011(1).zip`，大小28,051,967字节，SHA-256：
`07c5b03e056a0318af8c2368cc3a06c39ff2ef26c181b2912ef2f78f57084015`。
ChatGPT当前会话挂载的原件已由Python直接读取，ZIP CRC通过，Manifest包含6组Atlas、33个物种和132个独立PBR贴图，138个Atlas+贴图SHA-256全部匹配（0项错误）；其中15类为auto_candidate，7类review_only，11类fx_reference_only。

**跨环境阻断（需要真实日志支持）：** HWebCodex `import_conversation_files_to_project（会话附件导入远程工程）` 使用用户刚上传的精确文件ID再次尝试，仍返回 `requires trusted MCP host-file provenance`（缺少可信宿主文件来源）；替代的Library文件ID亦非该工具接受的OpenAiHostFileRef类型。Windows Runner遍历项目Incoming及Windows下载目录仍没有找到ZIP。因此**本次没有执行项目材质实例正式重导，UE5.8既有PBR母材质已保存但33个项目MI仍引用此前的旧纹理**。

`RunPBRAtlasImport.ps1（分批导入入口）`已经容许固定的原始ZIP名称及本次下载常见的`(1).zip`副本名称；用户只需将已上传的同一个ZIP下载并放入Windows工程既有Incoming目录，后续无需重新生成图片或资源清单。该入口默认只检查，不在原ZIP缺失时删除/覆盖旧资源。正式导入仍应执行3批用户授权的15个独立物种候选，其他18类需独立人工品质审查。

视觉及运行门禁保持未通过：Niagara实际颜色、地表穿插、粒子尺寸、角色预览和客户端Cook不可仅凭ZIP校验宣称通过。

## 八、2026-10-11 18:45 后续执行情况（用户授权覆盖旧材质）

- 已尝试通过HWebCodex接收用户新上传的精确 ZIP，插件仍返回`requires trusted MCP host-file provenance（缺少可信宿主文件来源）`；Windows工作区与下载目录均未发现原图ZIP。拒绝使用未授权公网中转、Base64文本分块或虚构资产覆盖。
- 本次通过当前聊天工作容器直接读取真实ZIP，验证压缩包SHA-256=`07c5b03e056a0318af8c2368cc3a06c39ff2ef26c181b2912ef2f78f57084015`，141项存档成员、6张图集、33套132张PBR纹理及Manifest物理尺寸全部符合格式；无错误CRC。上述文件校验不等于Windows导入。
- 已修复并加固`PreflightPBRAtlases.py（用户批准图集来源预检）`：固定ZIP哈希、独立PNG 512像素/8位RGB-Alpha与灰度通道核对、15个合格导入候选上限，以及重复执行时暂存Manifest冲突拒绝。
- `ImportPBRAtlasAssetsUE58.py（UE材质替换工具）`现在必须读取AssetImportTask返回的`imported_object_paths（实际导入路径集合）`，否则明确失败；同时记录每份旧Texture2D和MaterialInstance的备份路径与SHA-256，断点重跑新增通道资产时也需先备份。
- `RunPBRAtlasImport.ps1（安全分批入口）`已强制覆旧时携带`-Batch 1/2/3`，三批日志独立保存；`Tests/Assets/TestAmbientPBRAtlasPipeline.py（导入安全回归）`已增补检查，共21项静态检查通过，Python编译/PowerShell解析及本次目标Git差异检查全部成功。
- **本轮未实际替换旧材质，也未开始客户端Cook或角色工作室视觉验收。** 唯一待用户动作：把本次已经上传聊天的ZIP保存到Windows项目`SourceArt/AmbientNature/Incoming（待导入文件夹）`。文件到达后先只读预检，然后三个批次执行备份覆旧；不能把用户二次聊天上传误报为Windows已收到ZIP。

## 九、最新安全增量（2026-10-11 晚间）

- 当前会话重新直接验证上传ZIP：6张原始图集及132张贴图的SHA-256与8位PNG通道均通过，共33套，15套已初审可参与自动覆盖，其余18套继续阻断。本轮官方HWebCodex文件导入接口分别采用用户文件ID、宿主文件路径和结构化原始文件引用尝试后，都返回可信来源校验失败；Windows项目Incoming仍无ZIP，**本轮零份项目PBR材质被新图集覆盖**。
- 新增`StagePBRFromDownloads.ps1（Windows下载目录可信自动接收入口）`：用户仅需从本会话下载ZIP到本机Downloads；随后HWebCodex可在Windows直接执行SHA-256匹配、原子复制到固定工程Incoming，发现不同版本拒绝覆盖。实际在ZIP尚未下载到Windows时运行，返回`PBR_SOURCE_WAITING_FOR_WINDOWS_DOWNLOAD`，并已打开本机下载目录。
- `RunPBRAtlasImport.ps1（PBR批次导入）`现已集成上述自动接收；使用`-Apply`覆盖前检测`UnrealEditor.exe（虚幻编辑器）`及其他编辑器实例，避免并发写入同一.uasset。当前用户有运行中的编辑器进程，**必须先手动保存并正常关闭才能覆旧**，工具不会强行关闭。
- E盘仅余约1.6GB、D盘约162GB，导入脚本在E盘不足3GB时将暂存解包内容与旧资产备份放到D盘`D:\DivineBeastsWorkspace_PBR_WorkCache_20261011（非项目临时缓存）`；**目标.uasset仍写固定DivineBeastsWorkspace**。若项目盘低于1.2GB或备份盘低于700MB则拒绝覆盖。
- 新增`AmbientPBRPhysicalScaleManifest.json（33种物种视觉尺寸清单，单位cm）`；原ZIP内的物种尺寸、导入权限必须与清单一致才允许覆盖。该清单明确`niagaraSizeApplied=false`：材质贴图更新并不能自动调节Niagara的Particles.SpriteSize，形状/大小需后续单独调参和实机审核。
- `Tests/Assets/TestAmbientPBRAtlasPipeline.py（PBR流水线静态门禁）`增加数据身份、备份冲突、下载自动接收、编辑器占用、临时磁盘分流检查。当前实际Windows验证：29项通过、0失败，Python编译与PowerShell 5.1脚本解析通过；仅证明流程安全，不能当作图集已进入UE或Niagara效果合格。
