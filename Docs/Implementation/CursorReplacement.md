# SSLM 01经典银钢鼠标指针接入记录

当前状态：**经典银钢光标已接入项目。独立视口 RHI、客户端 22 资源入包和专服纯表现资源剥离均通过。**

## 一、范围与唯一配置

本次按用户选择的01经典银钢方向制作十态指针，沿用银钢、米白皮革、克制金边和蓝色宝石。新源图已由图像生成工具制作并精修透明背景；既有方案原图下载受阻，尚无逐像素对照证据。

唯一配置为 `Tools/Unreal/UI/CursorTheme.json`。其中维护源图SHA256、实际像素裁切边界、显示尺寸、热点、原生枚举映射和资源路径。作者读取该配置，不另设第二套状态表。

源图位于 `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/SourceArt/UI/Cursors/T_DBA_CursorAtlas.png`，1983×793 RGBA，5列2行。显示画布为 44×44 逻辑像素，其中透明留边为热点偏移保留空间。1:1 原生 Slate 渲染中，普通指针主体实测 30×31 像素，十态最长边为 28–32 像素。

十态为普通、拾取、攻击、交互、对话、采集、交易、拖拽、目标选择、禁止。背景和圆环内部保留透明Alpha。源图极低Alpha的杂色残留由UI材质的0.02下限过滤；不调整源RGB、饱和度或Gamma来迁就截图。

## 二、三层与插件归属

复用引擎 `UUserInterfaceSettings.SoftwareCursors` 和现有 `CommonGameViewportClient`，不新增Subsystem、轮询器、资产管理器或指针执行框架。

指针WBP继承既有 `/Script/GamePlatformUIClient.GamePlatformWidgetBase`，使用平台普通控件的统一生命周期；不进入激活栈、不获得焦点、不参加命中测试、不启用窗口拖拽。项目仅提供数据和视觉资产，全部归属既有第三层 `DBAUIPack_Core`。本次没有MOBA专属扩展职责，故不建立空中间层。

`DBAUIPack_Core.uplugin` 显式声明 GamePlatformUI 的 Game/Client/Editor 依赖。原生 AssetRegistry 核验了各控件对平台父类和对应材质的硬引用。运行配置通过 `bLoadWidgetsOnDedicatedServer=False` 跳过专服控件加载，纯表现包沿用现有服务器剥离策略。

Monolith 已创建并保存 1 张 Texture2D 图集、1 个 UI 母材质、10 个材质实例和 10 个 WBP。资源位于 `/DBAUIPack_Core/UI/Cursors/` 下的 `Textures`、`Materials` 和 `Widgets`。原生导入、编译回执和依赖记录位于 `Game/Saved/Validation/CursorReplacement`。

Texture2D 采用 sRGB、UI 纹理组和无 Mip，保留线性 Alpha 并以白色乘色。母材质分开输出 RGB 和透明度。材质实例按实际像素边界裁切并内缩到像素中心，避免邻格串色。

## 三、状态语义与热点

保留 `DivineBeastsPlayerControlsComponent::RefreshPointerFeedback` 的现有事实：Default表示普通，Hand表示可交互，SlashedCircle表示禁用交互，Crosshairs只表示可选目标。不得把Crosshairs改成攻击剑，避免友方和中性单位误示攻击。

闭合拖拽资源可供引擎的GrabHandClosed类型显示；本次不新增拖动输入规则。拾取、攻击、对话、采集和交易素材先备齐，未来只消费现有明确业务事实，不借用Resize或EyeDropper枚举伪装业务状态，也不根据Pawn身份猜测攻击资格。

引擎将软件指针控件中心置于鼠标位置，因此图像偏移为：

`RenderTranslation = DisplaySize / 2 - HotspotInCell * DisplaySize / CellSize`

普通使用指尖、攻击使用剑尖、选择和禁止使用圆环中心。WBP固定尺寸，Image和根SizeBox都为HitTestInvisible，白色Brush与ColorAndOpacity，避免遮挡真实控件输入。

## 四、运行映射与验收

`Game/Config/DefaultEngine.ini` 的原生软件光标表从 `CursorTheme.json` 生成。五种原生类型分别对应普通、交互、闭合抓手、目标选择及禁止。其余五种素材有独立资源和预览，业务触发需消费已有的明确交互事实。

现有 `DivineBeasts.UI.Color.SourceToSlate` 用独立编辑器进程冷加载已保存的光标，经过 CommonUI 视口的原生软件光标映射后比较实际 RHI 像素。Slate 会把图像顶点对齐到整数像素，像素参考使用相同规则。尺寸、轮廓与颜色的结果在自动化报告中逐项登记。客户端 IoStore 门禁从同一合同要求全部 22 个包的导出数据。

首轮 UBT 权限和断链故障已恢复。四个失效构建链接已在工程内归档，常规构建在原有工程目录内重新生成中间文件。旧失败证据和本轮 RHI、Cook、Stage 报告的索引保存在 `Saved/Validation/CursorReplacement20261107`。独立视口验证、实际包内容和人工联网鼠标验收分别记录。

最终结果见 `Saved/Validation/CursorReplacement20261107/Delivery.json`。独立 RHI 十态透明轮廓重合度为 0.998–1.000，平均线性 RGB 误差最高为 0.0056。客户端实际容器包含 22 个光标包，专服检查覆盖 3 个容器。游戏内鼠标的人工操作验收待进行。
