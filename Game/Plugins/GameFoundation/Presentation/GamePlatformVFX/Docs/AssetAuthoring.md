# AssetAuthoring（VFX资产制作说明）

本包不包含伪造 `.uasset`。请在 UE5.8 中创建真实资源。

第一批建议：

- `FXS_GPVFX_Instant_Generic`
- `FXS_GPVFX_Attached_Generic`
- `FXS_GPVFX_Projectile_Generic`
- `FXS_GPVFX_Beam_Generic`
- `FXS_GPVFX_Area_Generic`
- `FXS_GPVFX_Shield_Generic`
- `FXS_GPVFX_Portal_Generic`
- `FXS_GPVFX_Trail_Generic`

统一 User Parameters（用户参数）建议：

- `User.VFX.PrimaryColor`
- `User.VFX.SecondaryColor`
- `User.VFX.Intensity`
- `User.VFX.Scale`
- `User.VFX.Charge01`
- `User.VFX.Progress01`
- `User.VFX.SourcePosition`
- `User.VFX.TargetPosition`
- `User.VFX.TargetRadius`

Niagara 组件池优先使用引擎原生 Pooling Method；性能裁剪优先使用 Niagara Effect Type / Scalability。
