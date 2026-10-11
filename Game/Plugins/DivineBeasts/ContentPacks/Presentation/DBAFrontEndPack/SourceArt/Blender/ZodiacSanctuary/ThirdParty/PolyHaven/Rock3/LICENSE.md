# Rock 3 真实岩石 PBR 来源与许可

- 来源：[Poly Haven Rock 3](https://polyhaven.com/a/rock_3)，作者 Rob Tuytel。
- 许可：[Poly Haven CC0](https://polyhaven.com/license)，可商用及随项目再分发；不将网页预览或商标作为素材。
- 获取日期：2026-10-11；原始分辨率 2048×2048；纹理实际覆盖宽度约 1.5 米。
- 基色：`T_DBA_Sanctuary_Rock.png`，原文件 `rock_3_diff_2k.png`，MD5 `6ce05e52225a076d7ec67a54ce07f6ab`。
- OpenGL 法线：`T_DBA_Sanctuary_Rock_Normal.png`，原文件 `rock_3_nor_gl_2k.png`，MD5 `9c894aae6ffa61e94209b8551f87e5d8`。
- 原始 ARM 控制图：`T_DBA_Sanctuary_Rock_ORM.png`，原文件 `rock_3_arm_2k.png`，MD5 `8bae2ecb2ea2c1a6eba778628f631e9c`；R=AO、G=真实粗糙度、B=金属度。

仅为项目稳定英文身份改文件名，未修改 PNG 字节、颜色或锐化。下载入口为 `Tools/Blender/FrontEnd/DownloadRock3Textures.ps1`。
基色 sRGB；法线/控制图线性。UE 翻转法线绿通道以转换 OpenGL/DirectX 约定，不修改原图。
只用于第三层圣境岩石、远山与雕塑；不自动覆盖玉石圆盘、金饰或用户界面纹理。
