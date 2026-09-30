# Naming（命名规范）

代码：GamePlatformVFX（插件）、GamePlatformVFXClient（运行模块）、GamePlatformVFXEditor（编辑器模块）。

资产前缀：
- FXS_：Niagara System（Niagara系统）
- FXE_：Niagara Emitter（Niagara发射器）
- FXM_：Niagara Module Script（Niagara模块脚本）
- FXT_：Niagara Effect Type（Niagara特效类型）
- M_ / MI_ / MF_：材质 / 材质实例 / 材质函数
- T_：纹理
- SM_：静态网格
- DA_VFX_：VFX Definition（VFX定义资产）
- DA_VFXCAT_：VFX Catalog（VFX目录资产）
- `T_GPVFX_*_Source`：平台通用 SourceArt（源美术）文件，只作为导入源，不是运行时资产。
- `FXM_GPVFX_*`：从通用 HLSL/运动原语封装的 Niagara Module Script（模块脚本）。
- `FXS_GPVFX_*`：技术母版 Niagara System，只提供跨项目结构，不表达最终项目美术主题。

平台技术资产使用 GPVFX 标识；《神兽联盟》项目资产使用 DBA 标识，但 DBA 资产不得进入本基础层插件。
