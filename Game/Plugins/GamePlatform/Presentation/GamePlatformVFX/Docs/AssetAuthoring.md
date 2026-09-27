# AssetAuthoring（VFX资产制作）

本仓库不会通过文本工具伪造 .uasset/.umap。Niagara、材质、纹理、网格、曲线和 Review（人工核验）地图必须由 Unreal Editor（虚幻编辑器）创建。

一期编辑器资产制作顺序建议：
1. 创建平台母材质和材质函数。
2. 创建 FXM_GPVFX_* Niagara Module Script（模块脚本）。
3. 创建 FXE_GPVFX_* 通用 Emitter（发射器）。
4. 创建 FXS_GPVFX_* 技术母 System（系统）。
5. 创建 FXT_GPVFX_Critical/Combat/Status/Ambient。
6. 创建少量 DA_VFX_Platform_* Fallback Definition（降级定义）。
7. 创建 DA_VFXCAT_Platform_Default。
8. 创建 L_GPVFX_Review 测试地图并按质量档人工核验。

平台插件只保存无《神兽联盟》美术身份的技术资产。
