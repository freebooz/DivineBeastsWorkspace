# 神兽联盟自然远古界面定稿

2026-10-11用户明确将最终风格确定为自然、藤蔓、树叶、碧绿、原始与远古；此决定替代此前商代青铜主题。菜单按钮参考用户提供的石雕、根系、叶片与玉石横向牌匾。角色创建和选择仍沿用已确认的现代MMO布局，中心三维角色预览、鼠标旋转和动画职责不变。

## 资源与三层职责

全部视觉源图和界面纹理由第三层真实内容包DBAUIPack_Core拥有。GamePlatformUI仅提供中立界面能力，项目C++适配、ViewModel和路由仍由DBAClient/DivineBeastsUIClient维护；非竞技登录与新手村HUD不增加MOBA依赖。原有反射身份、控件绑定名称和旧素材保留兼容，不因视觉换肤移动英雄资源或地图。

源图位于`Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/SourceArt/UI/Nature/`：

| 源图 | 引擎资产 | 用途 |
| --- | --- | --- |
| `NatureMenuButton.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureMenuButton` | 无文字横向菜单底图，远古石兽、藤蔓、叶片与玉石；文案由按钮子控件呈现 |
| `NaturePanelFrame.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NaturePanelFrame` | 中部透明的藤蔓叶片边框，围绕表单、信息与HUD显示区 |
| `NatureLoginBackdrop.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureLoginBackdrop` | 原始森林、石雕、碧绿水光和远古遗迹的登录背景 |
| `NatureHUDFrame.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureHUDFrame` | 轻量横向根系与玉石细边框，避免纵向装饰拉宽后遮挡状态栏 |
| `NatureMinimapFrame.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureMinimapFrame` | 圆形藤蔓、玉石、守护兽和鹿角图腾；图内与图外透明，地图与文字实时叠加 |
| `NatureAbilityDock.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureAbilityDock` | 后续技能栏参考图定稿的一体式石兽藤蔓底座；头像、数字和技能均实时叠加 |

六份PNG由本次图像生成工具制作；引擎纹理导入、Widget调整、编译、保存与回读只能通过Monolith MCP执行。按钮底图不烘焙“开始”等文字，避免不同命令文案失真。用户已确认的神兽联盟LOGO继续使用。所有字体和用户名、密码控件的尺寸保持固定；仅画面背景铺满视口，装饰和主要按钮按固定尺寸布局。

## 技能栏参考图增量

后续用户明确提供一体式技能栏参考。底部以固定880×293装饰容纳左侧真实肖像/等级、上方绿色生命与蓝色气势、五个技能槽；中央为终极技能，顺序为普攻、主动一、终极、主动二、被动。快捷键和冷却继续来自实际已授予技能投影，不将示例Q/W/R/E/D写为未经绑定的输入。未接入的药水、卷轴和数量不制作虚假交互。右上小地图保留，状态效果按真实事件出现。

`WBP_DBA_UI_MomentumBar`是平台资源条基类的纯视觉实例，蓝色区分气势，保持既有字体11像素；不增加资源类型或玩法。平台资源条事件现在可显示`ResourceValueText`，只显示可信有限快照；未绑定、隐藏或非法数值时清空，不以资产文字伪造生命值。

## 小地图参考图增量

圆形地图窗采用引擎原生RoundedBox纹理裁剪，184×184地图画面叠加270×270透明装饰，N/W/E/S为独立固定14字号控件，标题保持16字号，玩家箭头保持18字号及24×24边界。标记中心限制为半径0.40，中心到圆边至少18.4像素，大于标记半对角线约17像素，因此完整标记留在圆内。缩放只裁剪真实底图UV，不缩放文本或整组Widget。

加号、减号与复位是本地1/2/4倍视野命令：角色和底图未就绪时禁用；倍率边界禁用对应命令。地图朝北，玩家箭头随真实角色朝向旋转。位置仍来自新手村俯视底图的真实厘米投影；不使用参考图的虚构山川、建筑或兴趣点。绑定代次、同步蓝图回调后的复核、请求取消和按钮退订保证换世界不显示旧视野。公共平台只接收中立Zoom/位置快照，项目地图路径留在第三层。

## HUD旅行实现

平台`PrepareForTravel`通过`ClearHUD`清理上一世界控件，会将预置CombatHUD从HUDLayer中移除。原实现只改Visibility，因此旅行后真实头像、地图与技能组合无法重新显示。项目根布局现在通过本地PlayerController/Pawn事件恢复自己的已有CombatHUD实例，并明确传播当前或空的PlayerContext；不恢复上一世界临时HUD，不创建第二个HUD，不以Tick轮询状态。

技能条和玩家状态面板记录实际订阅的控制器，换代或退出时解绑旧来源。断开时清空上下文和旧角色显示数据。头像、小地图与技能只消费真实本地角色和复制状态，视觉资源失败不影响服务器权威。

## 验证边界

真实保存的RootLayout参与自动化回归：清空HUD后重新挂载、临时控件不复活、重复事件只保留一个组合、控制器换代解除旧委托、断开清空上下文。用户此前的笼统“HUD可见”反馈随后被“没有看到HUD”纠正，不能把旧反馈当成这次修复的旅行验收。

编译、Monolith保存重载、包内资源检查与客户端人工验收分别记录在内容包`Docs/MonolithGenerationManifest.json`及本轮Saved证据中。编辑器界面预览只证明静态布局，不证明客户端已经登录、完成旅行或联机技能机制通过。游戏窗口按用户“仅检查代码与日志”的限制保留人工操作。

## 原稿色彩一致性补充（最终约束）

用户要求色彩与原稿一致，并明确本轮先校正全部界面。四份原稿按原字节保存为SourceArt/UI/Nature/NatureMenuReference.png、NatureAbilityReference.png、NatureMinimapReference.png、NaturePlayerFrameReference.png，来源与保存副本哈希一致。对应T_DBA_Nature*Reference由Monolith导入为sRGB、TC_EditorIcon无损、NoMipmaps、TEXTUREGROUP_UI；不再用重新生成的装饰代替原稿颜色。

UI/Materials/Nature/M_DBA_Nature{MenuReference,AbilityReference,MinimapReference,PanelReference,SkillFrameReference}由Monolith创建为UI/Translucent/Unlit。RGB只取样原稿，不做色调、亮度、饱和度或乘色变换；Alpha只移除深色底和固定生命/气势、肖像、技能、快捷键、地图及库存示意。菜单无字区域取样邻近原稿石板，面板边缘和技能框也复用原稿。HLSL、资源身份和边界见内容包Docs/NatureReferenceMaterials.json。

当前英雄图标、肖像、地图仍由其真实资产决定。文字和输入尺寸固定；不同视口采样、透明边缘合成和显示器不能由资源哈希证明最终截图逐像素相同，最终客户端观感待人工核验。本轮不调整场景、角色材质或光照。

## 玩家头像框定稿增量

左上角采用第四张原稿的固定380×214布局：圆形当前角色头像、玩家名、绿色生命、蓝色气势、等级和英雄图腾。`WBP_DBA_UI_PlayerFramePortrait`与`WBP_DBA_UI_PlayerFrameStatus`是原有项目父类的纯内容变体；两份PlayerFrame资源条仍继承平台中立资源条，全部由Monolith复制、布局、编译、保存和重载。`M_DBA_NaturePlayerFrameReference`直接保留原稿RGB，去除示意白马、固定数字及“图腾印记”文字；两份PlayerFrame填充材质复用原稿无字晶纹。

CombatHUD中的PlayerPortrait/PlayerStatus显示左上卡片，ActionBarPortrait/ActionBarStatus保留底部技能栏读数。两个位置各自拥有显示订阅和纹理请求，共用同一本地Pawn/ASC事实，不重复创建角色或维护权威状态。十二个英雄肖像由所属英雄包提供，英雄图腾复用同一角色肖像，仅为身份装饰，不恢复已取消的印记玩法。

名字仅在当前CharacterId与HeroId都匹配已选角色摘要时采用玩家输入名，否则显示英雄本地化名称。角色组件和持久摘要当前没有可信等级字段，等级数字清空并显示“—”，不使用图片60或固定1。换绑、取消、销毁清理主肖像和图腾；同步蓝图通知后核对代次，避免旧调用隐藏或覆盖新绑定。所有文字字号保持固定，底部技能槽按原稿位置改为72像素石框，不以窗口比例缩放字体。

原六份生成素材保留兼容和制作记录；当前装饰使用四张原稿和十一份取样材质。主题资产新增UI.Style.Text.Menu的绿色文案规则；DefaultThemeDefinitionId保持既有空值，当前运行外观由已保存控件及样式直接提供，不将未启用主题服务冒充运行验证。

## 登录面板缩小与原稿颜色修正（2026-10-11）

第五份原稿NatureLoginPanelReference.png与用户附件逐字节一致，SHA256为59433DD92346BE6E7F7DC6BFDE7E912E7C63857B3324D25B7CD74D6FCBF82FF8。引擎纹理、M_DBA_NatureLoginPanelReference与BP_DBA_ButtonStyle_LoginOverlay仍归第三层DBAUIPack_Core，全部由Monolith维护。登录面板固定600×315，输入控件固定284×42，字号保持原值。面板中的金色登录字样直接采用原稿，透明按钮负责焦点和点击；不再用矩形补色覆盖原稿。

实查发现CSV形式的ColorAndOpacity赋值被工具转换为全零；改用#FFFFFFFF并回读确认。角色页边框、技能槽与技能底座恢复白色乘色，技能图标去除蓝色乘色，四份生命/气势资源条恢复白色FillColorAndOpacity。登录面板外缘去除低强度黑底杂点，而两个输入胶囊、左右圆盘内部强制不透明，避免场景背景透入造成绿金颜色偏移。RGB仍只采样原稿，不加入亮度、饱和度、色调或补偿Gamma变换。

密码眼睛按钮采用按住显示、释放/离开/页面失活恢复遮罩；提交与失活清空密码。密码不进入ViewModel状态、日志或资产。新增生命周期回归需独立执行；客户端编译不等于该测试运行。离屏Widget导出PNG存在Gamma转换差异，不据此反向调暗原稿；最终客户端观感仍须人工核验。

发布兼容补充：加载回归发现新父类BindWidget字段会与旧编辑器Cook的非版本化属性布局不一致。眼睛按钮改为在激活、输入和失活事件中按名称查询，由Widget树持有，保持父类序列化布局不变；不依赖逐帧扫描，也不另存密码。此修正需以新包真实加载及生命周期回归验收，第一次Cook虽通过资源门禁，但因加载失败不作为可运行版本。

最终发布验证：Client构建bbff4c02-ed65-4bfc-b858-6e7f047c2d9b通过；开发客户端Cook/Stage 15072c79-68ad-4472-8d68-d187276ec36d通过，IoStore必需312项、缺失0。真实包内NamedWidgetContract与PasswordRevealLifecycle两项通过，JSON报告有效；HTML模板未打包，不能声称HTML报告生成。19份保存Widget重开回读并编译错误0/警告0，12份取样材质验证问题0。两个客户端8652、5864已从新包进入正常登录页，账号输入框获得焦点，未自动登录。旧5b5ed331包加载失败已弃用；新编辑器DLL构建受其他编辑器占用而未通过，本轮不将其计为通过。最终客户端色彩及新手村旅行仍需人工确认。

本轮中文说明审核覆盖登录事件查询、瞬时密码释放、兼容反射布局、测试夹具、包资源门禁、原稿取样/透明处理、目录规划与Monolith清单。无新增插件或协议；未进行项目全量中文注释审核。
