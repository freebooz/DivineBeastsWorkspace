# DBAPresentationPack_Core（神兽联盟公共视觉内容包）

本内容包属于DivineBeasts项目第三层，只包含可复用的视觉资产和Definitions，不重复实现VFX播放、数据加载或缓存。

## 新手村基础草地（2026-10-13）

`Content/Textures/Terrain/Grass/T_DBA_Village_Grass_{BaseColor,Normal,ORM}`：三张Monolith真实导入的2K扫描PBR，来自既有CC0 `leafy_grass`源图。基础色sRGB、法线线性NormalMap、ORM线性Masks；不改原稿像素、不用滤色补偿导入。WorldPack共享材质只保存中性参数默认，DBAClient以Data世界租约绑定高清贴图，地图切换时释放自身需求。作者和资产证据在`Saved/Validation/VillageAmbient20261013`；新编译、运行与Cook尚未通过，不能宣称此目录已在当前人工客户端生效。当前32海鸥+24燕群设计沿用既有本包鸟类外观及平台GPU模板，不新增鸟Actor/播放器。

## 自然飘落物
- `SourceArt/FallingFoliage/Textures`：桃花、枫叶、竹叶、银杏叶的原创透明颜色和法线PNG，另附SHA-256清单。
- `Content/VFX/Environment/FallingFoliage/Textures`：实际导入的Texture2D资产。
- `Content/VFX/Environment/FallingFoliage/Materials`：继承平台通用母材质的四种材质实例。
- `Content/VFX/Environment/FallingFoliage/Niagara`：轻量外观系统变体，运动逻辑统一在GamePlatformVFX中。
- `Content/VFX/Environment/FallingFoliage/Definitions`：真实UGamePlatformVFXWorldDefinition资产；通过GamePlatformData按LogicalId租约加载。

第三层资源不被平台插件硬引用。正式场景由DBAFrontEndPack拥有，角色选择和角色创建共享预览工作室的同一环境播放请求。

四种`UGamePlatformVFXWorldDefinition（平台世界环境特效定义）`以`GamePlatformDefinition（平台主资产类型）`统一登记，`Game/Config/DefaultGame.ini`的主资产扫描目录必须包含本内容包的`VFX/Environment/FallingFoliage/Definitions`；FrontEndClient客户端专用Cook配置分别收录本内容包、通用GamePlatformVFX运动模板与前端地面锚点。缺任意一环只允许报告加载或目录失败，不得将编辑器中文件存在当成打包可用。

专用服务器不得装配本纯表现内容包；目标构建、Cook与Stage都须单独验证。UE .uasset / .umap必须由UE工具生成并重新加载通过。
## 扩展的自然环境粒子库（2026-10-11）

- `SourceArt/AmbientNature/AmbientNatureStyleCatalog.json（39种风格身份清单）`：4种既有落叶、33种新增项目视觉风格、2种由平台天气插件承接的雨雪；唯一StyleId、稳定Definition逻辑版本及归属记录。
- `SourceArt/AmbientNature/AmbientNatureAreaProfiles.json（21组候选区域风格/权重配置）`：前端角色工作室、新手村桃林与梨园、世界红枫坡/银杏院/梧桐大道等。**仅数据化候选，不代表地图已经激活这些特效**。
- `SourceArt/AmbientNature/Textures（颜色及法线源PNG）`：新增33种各两张共66张1024×1024透明贴图，配套`GeneratedTextureManifest.json（原始SHA-256清单）`和人工复核图。
- `Content/VFX/Environment/AmbientNature/Textures（66张实际Texture2D）`、`Materials（33个项目材质实例）`、`Niagara（33个项目外观变体）`、`Definitions（33份实际世界环境定义）`。该层只引用平台机制与平台材质，绝不由平台硬引用神兽联盟资源。
- 平台`GamePlatformVFX/Content/Environment/AmbientNature（游戏平台通用自然环境资源）`拥有`M_GP_AmbientNature_Soft（柔和半透明母材质）`、`M_GP_AmbientNature_Glow（发光母材质）`、`NS_GP_AmbientFloat_Base（基础漂浮粒子）`，后者暂复用雪花漂移作为第一版通用运动，不代表火星、萤火虫等的专属动效已完成。
- `UDivineBeastsAmbientRegionProfile（项目区域DataAsset类型）`的脚本`AuthorAmbientRegionProfilesUE58.py（真实编辑器资产生成器）`专门将候选区域落盘成真实引擎数据资产，只有Editor目标模块实际构建、编辑器真实保存后才可将其标记为完成。
- 39种风格不为每个资产新增代码插件或Niagara播放器。原有`FallingFoliage（飘落物）`系统/定义保持身份；雨雪依旧归原`GamePlatformWeather（平台天气）`与`GamePlatformVFX（统一执行）`。
- 来自当前对话的梨花、樱花、银杏、梧桐、枫叶高清参考图集导出为可人工导入的源包；远程连接器未能接受ImageGen原图文件引用，当前工程内33套新源图是独立程序绘制版本。待原图可信传入`SourceArt/AmbientNature/ExternalReferences`后，使用`ImportApprovedAmbientReferenceArtUE58.py（原图SHA校验与UE导入工具）`做可回滚的手工批准替换；任何未知来源图片不得冒充已交付原图。
- 实施细节、未完成门禁和证据见`Docs/Implementation/AmbientNatureAssetLibrary_20261011.md（自然环境资源库实施文档）`。正式验收仍需地图区域动态接入、Editor目标反射注册、区域DataAsset落盘、Cook/Stage、D3D12及移动端实际性能与人工视觉测试。

## Niagara PBR 落叶外观母版（2026-10-11）

平台 `GamePlatformVFX（游戏平台视觉特效插件）` 已通过 UE5.8 实际保存 `M_GP_AmbientNature_PBR_Foliage（通用双面植被PBR母材质）`，包含基础颜色、法线、粗糙度、独立OpacityMask（透明裁剪）、镜面反射与Subsurface（次表面透光），不硬引用本项目纹理。项目 `AmbientNature（自然环境内容）` 已登记33套 `MI_DBA_Ambient_*（项目材质实例）`和原 Niagara 变体，不另建播放框架。

本轮用户生成6张展示图集已拆分为33类/132张独立PBR参考贴图，带SHA-256与半自动质量门禁。**只有15类单体边缘完整的花瓣/叶片进入分批UE导入候选，截断花瓣和混合FX图不自动替换。** 由于Windows Runner尚未收到原图ZIP，当前项目材质实例仍使用此前纹理，不能声称参考图已经完整导入或在角色界面显示成功。

待用户把 `DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip（六图集独立贴图包）` 复制到 `SourceArt/AmbientNature/Incoming（待导入素材目录）`，使用 `Tools/Unreal/VFX/FallingFoliage/RunPBRAtlasImport.ps1（分批预检与UE导入入口）` 验证全部哈希、备份旧资产，并更新原材质实例路径；正式流程与运行验收见 `Docs/Implementation/AmbientPBRAtlasImport_20261011.md（PBR图集工程导入说明）`。
