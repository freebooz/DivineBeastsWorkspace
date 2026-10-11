# 用户界面原稿颜色导入与验证

日期：2026-10-11。平台提供中立UI机制；神兽联盟原图、材质和样式归第三层DBAUIPack_Core，回归源码归DBAClient客户端模块Private/Tests。所有界面资产操作只经Monolith MCP。

## 本次查明的原因

- 新面板彩色原稿被设为SRGB=false，与其他彩色原稿的true不一致。8位设计图是sRGB编码值，不能当线性光强交给UI材质；遮罩是线性数据，二者必须分别设置。
- 仅回读sRGB布尔字段不够。本次重新导入并重建材质图、匹配采样器后渲染结果才改变，必须检查GPU结果。
- 锁定Monolith 0.23.0的capture_scene_preview(widget)使用开启Gamma的FWidgetRenderer，同时创建sRGB目标，导致该路径的PNG额外编码。同一页面原稿取样约(166,174,144)，错误设置预览约(235,237,228)，修复后约(212,216,199)，后者符合再次sRGB编码约(211,215,198)。此预览只作布局证据，不能据此反向调暗正式纹理。
- 动态技能图标曾将线性亮度与调色纹理再次相乘，导致变暗，已改为原稿取样颜色插值。业务禁用/冷却/气势不足仍需真实显示，不能伪造可用状态。
- 画刷和文字乘色是线性颜色，设计十六进制颜色不能直接当线性值写入。原稿图像保持白色乘色、正常透明度；遮罩只管理Alpha，不能用RGB亮度裁掉原稿暗部。

数值对比允许双线性缩小误差，不代表最终游戏截图已经逐像素验收。

## 固定导入合同

| 用途 | sRGB | 设置与约束 |
| --- | --- | --- |
| 彩色原稿、LOGO | 开启 | TC_EditorIcon、UI组、NoMipmaps，实际B8G8R8A8；源哈希不变，亮度/饱和度/RGB曲线为1、色相0，匹配Color采样器 |
| 独立Alpha遮罩 | 关闭 | 无损线性数据采样，仅使用Alpha，不替换原稿RGB |
| 真实小地图、动态头像、业务图标 | 按明确来源合同 | 捕获编码、Alpha和业务状态单独验证，不能套用遮罩规则 |

DBAUIPack_Core/Docs/UIColorImportManifest.json登记当前13份源文件的角色、SHA-256及稳定身份。Tools/Unreal/UI/ImportDBAThemeTexturesMonolith.py已改为离线请求生成器，只读源文件并输出Monolith动作，不编辑图片或UE二进制。新图必须明确Color/AlphaMask，旧Theme清单可显式传入；不再因ALREADY_IMPORTED跳过设置检查。

Monolith导入动作不接受EditorIcon参数，必须原生导入后用严格属性动作设置TC_EditorIcon、NoMipmaps和sRGB，回读实际格式，再保存重载。错误必须停止该项交付。

## 验收顺序

1. 源哈希一致，保留RGB，不靠重画或调亮/调暗补偿显示错误。
2. 确认Monolith工程和版本，执行动作，回读纹理/材质采样器、白色乘色及Alpha，编译保存重载。
3. 运行Tests/Assets/TestUIColorImportPolicy.py，验证颜色/遮罩分离、错误哈希和跨层目标拒绝。
4. 用真实RHI运行DivineBeasts.UI.Color.SourceToSlate；读取线性浮点GPU结果并与真实原稿像素解码值比较，不依赖有额外编码的预览PNG，不操作人工客户端。
5. Cook后核验新产物和资产，再由用户验收最终显示。编辑器回归、布局PNG均不能代替最终客户端验收。

最新LOGO原稿2172×724，SHA-256为ca7704dd63b5580c8a255009e6b17a5366cc94a7ee88b18821b6084a30fef910，沿用/DBAUIPack_Core/UI/Textures/DBA_MythicLogo。登录420×140，角色选择240×80。暂保留原稿深色背景，不以生成图改写RGB。

不声称全部存量图像或最终客户端已经视觉验收。本轮GPU结果须以实际测试报告为准。

封面增量：用户新图1672×941，哈希87e6ac3803f3a0530eb8e05f4039f37b0171d42985f9dc52a16cad663d6bd227；复用NatureLoginBackdrop稳定身份。仅背景在CoverBackgroundScale中等比铺满并裁切，LOGO、文字、账号密码和按钮是根布局的独立固定控件。纹理与登录WBP经Monolith编译保存；用户明确保留共享编辑器，重启/最终客户端验证延期。
