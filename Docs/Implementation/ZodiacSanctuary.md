# 神兽圣境角色选择／创建三维场景

## 已实现范围与归属

2026-10-11按用户参考图制作可编辑三维初版：生肖圆盘、台阶、月门古树、石兽、岩台瀑布、荷叶莲花及远山宫殿。场景不是原图贴在平面上的背景。
17个分件、六尊石兽和14种PBR材质由Blender 5.2.1制作；UE5.8通过Monolith v0.23.0导入真实资产。
所有新增内容归第三层已有`DBAFrontEndPack`，不新增插件、服务器角色、流程执行器或另一套角色资产。

稳定入口仍为`/DBAFrontEndPack/Maps/L_DBA_CharacterStudio`；选择与创建两页沿用同一个本地玩家预览子系统和平台舞台。
舞台/角色原点保持不变，环境整体缩放0.45，角色相机为(650,0,340)厘米、俯仰-8度、视角90度。
旧Landscape只隐藏，未删除；保留`DBA.Foliage.Ground`锚点与自然飘落机制。首次修改前的UMAP备份在`Game/Saved/Validation/ZodiacSanctuary/BeforeSanctuary`。

## 纹理与材质

真实岩石使用[Poly Haven Rock 3](https://polyhaven.com/a/rock_3)的2K基色、OpenGL法线及ARM图，作者Rob Tuytel，[CC0许可](https://polyhaven.com/license)。原图未改像素，下载MD5校验通过；许可及原文件身份随SourceArt交付。
岩壁按主法线投影UV，减少垂直拉伸；真实岩石用于石台、雕塑和远山，玉石圆盘/金饰仍独立管理。
玉石、苔藓、树皮和叶片使用原创低对比基色、法线、ORM，禁止人工颗粒与锐化。法线/ORM采用线性，颜色图采用sRGB；UE显式翻转法线绿通道。
原生PBR节点提供表面粗糙度、金属度和薄抛光层参数，默认白色乘色保留原图RGB；水幕与水面GPU时间驱动低频材质变化，不轮询业务状态。
本次共15张当前引用的纹理；先期3张程序化石材纹理保留兼容，但当前石材已使用真实岩石。容器中圣境目录共49个资产导出项。

## 制作、导入与检查入口

- `Tools/Blender/FrontEnd/DownloadRock3Textures.ps1`：下载固定真实岩石纹理并核验哈希。
- `Tools/Blender/FrontEnd/CreateZodiacSanctuary.py`：从真正空场景生成BLEND、17份FBX、源PBR纹理和制作镜头；手改制作件须另存后再重跑。
- `Tools/Unreal/FrontEnd/ImportZodiacSanctuary.py`：经Monolith分批导入、PBR重建、旧图备份及原位装配；未保存地图或PIE时拒绝修改。
- `Tests/Assets/ValidateZodiacSanctuaryBlender.py`：从空会话重开保存BLEND，核对网格、六兽、UV、法线/ORM、落脚区和渲染图。
- `Tests/Assets/ValidateZodiacSanctuaryUnreal.py`：重载工作室后核对17个场景Actor、14种材质、15张纹理、相机轴向和舞台/锚点。
- `Tests/Assets/TestZodiacSanctuaryContainer.ps1`：核对实际IoStore中的47个必要资产，包括地图与完整PBR引用闭包。

## 本轮实际证据与限制

保存BLEND独立重开通过；UE工作室保存及重载检查通过；PIE从前端宿主动态流送场景，`Hero.Zodiac.Tiger`真实外观网格加载，镜头绑定舞台；失活后恢复原观测Pawn。
这是开发态预览验证，没有提交账号密码、创建真实角色或通过后端准入，不等同登录至新手村端到端验收。
客户端Cook/Stage及既有134项IoStore门禁退出0，最终发布运行ID为`6a27b3ba-9d5d-44a6-aee9-a544839f4c71`；证据归`Saved/Validation/FoundationM0`。圣境专用47项容器检查另行执行并保留报告。
没有改动C++，使用已有Client二进制重新发布资源；未执行新的服务器Cook、移动设备适配或发布版交互验收。
当前是可运行场景初版，不宣称达到参考图电影级成品：石兽形体、岩台轮廓、雾与瀑布、当前自然飘落的亮度/粒径仍需人工精修和视觉验收。保存件、原始材质与客户端包均可复核，不以渲染图替代最终验收。
