# CH02 MagicGlow 纹理生成提示词

本批纹理使用内置图像生成工具创建 1024×1024 制作母版，并输出 512×512 移动端运行版本。所有数据纹理均为纯黑底白灰数据；Noise、CloudNoise 与 FresnelBreakup 经过镜像无缝化；Ripple 使用纯洋红键色生成后转换为真实 Alpha。

## T_VFX_Energy_Noise_01.png

柔和大尺度魔法云纹能量噪声，包含清晰的大、中、小三级频率；低频形成整体流动节奏，中频形成云状块面，高频仅少量点缀。纯黑底白灰数据，信息分布均匀，四边无缝，可用于材质重着色、扰动与溶解。避免 1px 噪点、发丝纹和压缩闪烁。

## T_VFX_Energy_Noise_02.png

比云噪声更细密、更锐利的能量扰动纹理，采用中等尺寸的角状湍流结构，保持大中小频率层级，同时保证 ASTC/ETC 压缩稳定。纯黑底白灰数据，四边无缝，无中心主体。

## T_VFX_Energy_CloudNoise_01.png

圆润魔法云团密度纹理，中心与边缘具有丰富灰度变化；宽阔低频云团、清晰中频云岛与少量高频破碎共同构成可控数据。纯黑底白灰数据，四边无缝，缩小后仍可读。

## T_VFX_Energy_Ring_01.png

严格居中的单个白色圆环 Mask，黑色空心与黑色外围，内外边界清晰且抗锯齿，环宽适合移动端压缩。正交二维、完美圆形、无透视、无装饰、无辉光。

## T_VFX_Energy_RingSoft_01.png

严格居中的柔和能量光环，亮度在环带中心达到峰值，并向内外两侧平滑衰减到纯黑。圆环对称、灰度连续、无色彩、无条带，适合柔光和 Halo 材质。

## T_VFX_Energy_Ripple_01.png

三个居中的中性白能量波纹环，每道波峰具有清晰白色核心与柔和灰色光晕，不包含中心光球。主体限制在画布 62% 直径内，洋红键色背景生成后转换为透明 Alpha，四周留出至少 19% 安全区。

## T_VFX_Energy_Gradient_01.png

中心纯白高能核心，向外单调平滑过渡为浅灰、中灰、深灰并最终衰减到纯黑。严格圆形、无圆环、无射线、无噪点，适合作为能量核心和径向遮罩。

## T_VFX_Common_FresnelBreakup_01.png

用于打散完整 Fresnel 的不规则弧状破碎数据：宽阔断裂曲带、明确黑色间隙、灰色次级碎块与集中的白色亮部。图案不形成单一完整圆环，四边无缝，结构适合移动端读取。

统一附加要求：production-ready, power-of-two texture, mobile-friendly, mip-safe, compression-safe, UE texture import safe, clean edges, grayscale shader data, no AI grain, no plastic look, no text, no logo, no watermark, no border, no scene, no floor, no UI, no camera perspective, no jpeg artifacts, no edge contamination。
