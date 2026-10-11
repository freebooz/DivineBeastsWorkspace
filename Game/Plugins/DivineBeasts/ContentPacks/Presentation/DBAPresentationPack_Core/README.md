# DBAPresentationPack_Core（神兽联盟公共视觉内容包）

本内容包属于DivineBeasts项目第三层，只包含可复用的视觉资产和Definitions，不重复实现VFX播放、数据加载或缓存。

## 自然飘落物
- `SourceArt/FallingFoliage/Textures`：桃花、枫叶、竹叶、银杏叶的原创透明颜色和法线PNG，另附SHA-256清单。
- `Content/VFX/Environment/FallingFoliage/Textures`：实际导入的Texture2D资产。
- `Content/VFX/Environment/FallingFoliage/Materials`：继承平台通用母材质的四种材质实例。
- `Content/VFX/Environment/FallingFoliage/Niagara`：轻量外观系统变体，运动逻辑统一在GamePlatformVFX中。
- `Content/VFX/Environment/FallingFoliage/Definitions`：真实UGamePlatformVFXWorldDefinition资产；通过GamePlatformData按LogicalId租约加载。

第三层资源不被平台插件硬引用。正式场景由DBAFrontEndPack拥有，角色选择和角色创建共享预览工作室的同一环境播放请求。

四种`UGamePlatformVFXWorldDefinition（平台世界环境特效定义）`以`GamePlatformDefinition（平台主资产类型）`统一登记，`Game/Config/DefaultGame.ini`的主资产扫描目录必须包含本内容包的`VFX/Environment/FallingFoliage/Definitions`；FrontEndClient客户端专用Cook配置分别收录本内容包、通用GamePlatformVFX运动模板与前端地面锚点。缺任意一环只允许报告加载或目录失败，不得将编辑器中文件存在当成打包可用。

专用服务器不得装配本纯表现内容包；目标构建、Cook与Stage都须单独验证。UE .uasset / .umap必须由UE工具生成并重新加载通过。
