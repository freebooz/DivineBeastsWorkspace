# VFXLib Common SourceArt（VFX Lib 通用源美术）

本目录保存从外部 VFX Lib 审查后选出的去主题化源素材，只用于 Unreal Editor 导入与人工追溯，不属于运行时 Content（内容）。

已迁入：Glint（闪光）、ShockRing（冲击环）、SoftCore（柔光核心）、Noise（噪声）、Mist（雾）、Halo（光晕）。

SourceManifest.json 保存目标文件的 SHA-256 与字节数。任何正式运行时纹理必须由 Unreal Editor 导入为 T_GPVFX_* 资产后，再通过 Niagara/Material/Definition 的软引用进入 VFXRuntime Asset Bundle（运行时资产包）。

禁止直接把 SourceArt 路径加入 Cook；Dedicated Server 产物中出现 T_GPVFX_*_Source 视为泄漏。
