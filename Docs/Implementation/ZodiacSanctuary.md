# 神兽圣境角色选择／创建三维场景

## 已实现范围与归属

2026-10-11按用户参考图制作可编辑三维初版：生肖圆盘、台阶、月门古树、石兽、岩台瀑布、荷叶莲花及远山宫殿。场景不是原图贴在平面上的背景。
17个分件、六尊石兽和14种PBR材质由Blender 5.2.1制作；UE5.8通过Monolith v0.23.0导入真实资产。
所有新增内容归第三层已有`DBAFrontEndPack`，不新增插件、服务器角色、流程执行器或另一套角色资产。

稳定入口仍为`/DBAFrontEndPack/Maps/L_DBA_CharacterStudio`；选择与创建两页沿用同一个本地玩家预览子系统和平台舞台。
舞台/角色原点保持不变，环境整体缩放0.45。2026-10-11按用户最新角色创建/选择参考图，角色相机改为(430,0,95)厘米、俯仰0度、水平视角55度；以16:9为纵向视场基准，原生MaintainYFOV约束保持窗口变形时的全身占比。原高处广角(650,0,340)、俯仰-8度、90度被替代，不再作为导入默认构图。
旧Landscape只隐藏，未删除；保留`DBA.Foliage.Ground`锚点与自然飘落机制。首次修改前的UMAP备份在`Game/Saved/Validation/ZodiacSanctuary/BeforeSanctuary`。

## 纹理与材质

真实岩石使用[Poly Haven Rock 3](https://polyhaven.com/a/rock_3)的2K基色、OpenGL法线及ARM图，作者Rob Tuytel，[CC0许可](https://polyhaven.com/license)。原图未改像素，下载MD5校验通过；许可及原文件身份随SourceArt交付。
岩壁按主法线投影UV，减少垂直拉伸；真实岩石用于石台、雕塑和远山，玉石圆盘/金饰仍独立管理。
玉石、苔藓、树皮和叶片使用原创低对比基色、法线、ORM，禁止人工颗粒与锐化。法线/ORM采用线性，颜色图采用sRGB；UE显式翻转法线绿通道。
原生PBR节点提供表面粗糙度、金属度和薄抛光层参数，默认白色乘色保留原图RGB；水幕与水面GPU时间驱动低频材质变化，不轮询业务状态。
ORM只实际消费R（AO）与G（粗糙度），B保留原始文件数据；本期金属度统一由`MetallicStrength`参数控制，不把未接入的B称为已生效倍率。
本次共15张当前引用的纹理；先期3张程序化石材纹理保留兼容，但当前石材已使用真实岩石。容器中圣境目录共49个资产导出项。

## 制作、导入与检查入口

- `Tools/Blender/FrontEnd/DownloadRock3Textures.ps1`：下载固定真实岩石纹理并核验哈希。
- `Tools/Blender/FrontEnd/CreateZodiacSanctuary.py`：从真正空场景生成BLEND、17份FBX、源PBR纹理和制作镜头；手改制作件须另存后再重跑。
- `Tools/Unreal/FrontEnd/ImportZodiacSanctuary.py`：经Monolith分批导入、PBR重建、旧图备份及原位装配；未保存地图或PIE时拒绝修改。
- `Tests/Assets/ValidateZodiacSanctuaryBlender.py`：从空会话重开保存BLEND，核对网格、六兽、UV、法线/ORM、落脚区和渲染图。
- `Tests/Assets/ValidateZodiacSanctuaryUnreal.py`：重载工作室后核对17个场景Actor、14种材质、15张纹理、相机轴向和舞台/锚点。
- `Tests/Assets/TestZodiacSanctuaryContainer.ps1`：核对实际IoStore中的47个必要资产，包括地图与完整PBR引用闭包。
- `Tests/Assets/TestZodiacSanctuary{ImportSafety,MaterialValidation,Backup}.py`：真实未保存材质保护、真实材质副本断链及不完整备份拒绝回归，独占开发测试资产精确清理，不修改正式资源。

## 本轮实际证据与限制

保存BLEND独立重开通过；UE工作室保存及重载检查通过；PIE从前端宿主动态流送场景，`Hero.Zodiac.Tiger`真实外观网格加载，镜头绑定舞台；失活后恢复原观测Pawn。
这是开发态预览验证，没有提交账号密码、创建真实角色或通过后端准入，不等同登录至新手村端到端验收。
客户端Cook/Stage及既有134项IoStore门禁退出0；2026-10-11随用户确认的透明LOGO重新发布当前资源，最新成功运行ID为`3cd1ac21-99cf-46be-a39c-d17d5c3de69d`，此前成功运行`6a27b3ba-9d5d-44a6-aee9-a544839f4c71`保留。圣境专用47项检查对新IoStore再次通过，报告为`Saved/Validation/UIBrandLogo/20261011-TransparentOriginal/ZodiacSanctuaryContainer.json`。
补充材质节点中文说明与导入安全门禁后，保存资产复核通过。早期重新打包的`043017cd-d3a6-4210-b076-6ee35b07a1c6`、`74f23f16-a339-4bd6-bb01-41fe2fd112d5`与`c22a0fda-0b3b-4c58-9ed1-a1c7a7b268e2`曾因共享引擎构建锁占用而退出2，未执行Cook，保留原结果；共享锁释放后最新重新发布退出0，已解除该项发布阻塞。打包闭包仍不替代人工视觉或联网验收。
没有改动C++，使用已有Client二进制重新发布资源；未执行新的服务器Cook、移动设备适配或发布版交互验收。
当前是可运行场景初版，不宣称达到参考图电影级成品：石兽形体、岩台轮廓、雾与瀑布、当前自然飘落的亮度/粒径仍需人工精修和视觉验收。保存件、原始材质与客户端包均可复核，不以渲染图替代最终验收。

## 导入工具安全复核整改

### 角色镜头参考图修正补证（2026-10-11）

仅经Monolith原位修改并保存工作室相机，修改前完整备份地图及外置文件；重载后17个场景网格、14种材质、15张纹理和舞台/锚点验证通过。导入与光照更新工具同步采用新镜头，防止重跑恢复旧广角。
独立PIE使用保存相机和十二真实外观，头脚包围盒均在画面内，纵向高度占比约76%，12个Idle实例正常初始化；同一舞台继续复用鼠标旋转。实际渲染图为`Saved/Validation/CharacterCamera/20261011-ReferenceView/ReferenceCameraPIE.png`，这是独立开发预览，不是人工登录客户端验收。
当前客户端资源发布编号`4ad45f59-3588-499d-8b2a-d714aed13a11`，351项实际IoStore资源与47项圣境闭包全部通过，发布包19项UI回归通过。没有修改C++、界面字号、输入大小、服务器或角色网格比例。人工登录与相机视觉结论仍以本轮流程证据及用户确认区分。

已复现并修复：未保存材质可被重跑覆盖、仅用备份目录存在判断完成、法线断链漏检及错误包名前缀误匹配。
导入现在在批次首次修改前检查全部目标脏包，单材质/单导入也独立防护；不保存无关用户脏包。
备份采用同盘临时目录、逐件SHA-256和完整标记后原子发布；已有不完整目录或缺文件/哈希错立即拒绝。首次原备份已与改图前`bfcab393`中的UMAP Git对象`48c0925c01129f859be1d41fe148ebf17ca57333`核对相同，原版无外置Actor，完整标记据此补录。
UE门禁核对真实BaseColor/Normal/Roughness/Metallic/抛光输入、纹理身份及ORM的R/G通道；容器检查要求完整目录段，不把`BackupDBAFrontEndPack`视为正式包。
真实脏材质、真实材质副本断开Normal、不完整备份及伪包名负向用例均按预期拒绝；正式保存资产与47项真实容器门禁继续通过。只读复审未发现剩余阻断问题。
最终检查另复现了测试清理未检查返回值的问题：Python包装器持有原生引用导致临时材质删除失败。测试现在先释放自身包装器，再按独占UUID身份删除，并核对返回值、资产注册表与脏包；成功标志在清理后输出。本轮五份独占测试残留已精确清理，其中唯一落盘原件备份至`Game/Saved/Validation/ZodiacSanctuary/TestResidueBackup`，未保存的新手村地形包保持原状。
早先编辑器验证Job达到3600秒运行上限后退出；恢复连接后已重新加载工作室并通过三项安全回归及保存资产检查，资产注册表无测试残留。未保存保护用例现在显式创建真实未保存编辑，且不把编辑器无法产生脏包的前置失败记为通过。退出前另有未保存的新手村地形层`LI_DBA_Terrain_Grass`，不属于本次资产；其恢复应由所属任务检查自动保存，不在本次任务中擅自覆盖保存件。
