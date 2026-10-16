# CH01 Magic Spark AI 纹理包

本目录包含 AI 原始输出和经过移动端规范化的最终 PNG。`Mobile_Ready` 才是建议导入 Unreal Engine 的目录。

## 目录

- `Raw_AI`：模型原始输出，仅用于追溯和二次制作。
- `Mobile_Ready`：2 次幂、规范通道和安全留白的最终纹理。
- `PROMPTS_ZH.md`：可直接追加到原始提示词的中文关键补充词，以及每张纹理的专用提示词。

## 移动端最终规格

| 文件 | 尺寸 | 通道 | UE 建议压缩 | Address |
|---|---:|---|---|---|
| Spark / SparkSharp | 1024×1024 | RGBA | Default，保留 Alpha | Clamp |
| Streak | 1024×512 | RGBA | Default，保留 Alpha | Clamp |
| Glint / Radial | 512×512 | RGBA | Default，保留 Alpha | Clamp |
| Noise / Distortion | 512×512 | 单通道灰度 | Grayscale | Wrap |

通用 UE 属性：`sRGB=false`、`Texture Group=Effects`、启用默认 Mip 生成。Sprite 不要导入为 `TC_Grayscale/G8`，否则 Alpha 会被移除。

## 材质连接

- Emissive：`Texture RGB × Particle Color RGB × HDR Intensity`。
- Opacity：`Texture A × Particle Color A × Opacity`。
- 使用 Straight Alpha，不要预乘 RGB。
- Sprite 使用 Clamp，避免 Mip 在边缘重复采样；Noise/Distortion 使用 Wrap。
- Additive 材质不需要写入深度；不要通过材质修改全局 Bloom 或 Lens Flare。

## 移动端注意事项

- 透明纹理的大面积柔光会增加 Overdraw；主体已限制在有效区域，Niagara 中仍应控制 Sprite Size 与存活数量。
- Glint、Radial 和 Noise 使用 512²，主爆点使用 1024²；Low 档可通过 Device Profile 增加 LODBias。
- Android 优先验证 ASTC；需要兼容低端设备时额外验证 ETC2 的 Alpha 边缘。
- 在 64px、128px、256px 三档预览检查射线闪烁、Alpha 黑边和 Mip 丢失。

## 生成说明

本批次使用内置图像生成工具逐张生成。透明类输出直接获得了 RGBA，因此未使用键色抠图；随后统一转为灰度 RGB＋独立 Alpha、裁剪有效内容并重新加入安全透明边距。Noise/Distortion 经过镜像拼接，保证画布四边连续。
