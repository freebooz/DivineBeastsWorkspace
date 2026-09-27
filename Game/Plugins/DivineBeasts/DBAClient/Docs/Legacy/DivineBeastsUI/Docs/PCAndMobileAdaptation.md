# PCAndMobileAdaptation（PC与移动端适配）

FDivineBeastsUIScreenCatalog 为Screen/HUD提供PC默认Widget软路径，并声明Android Widget variant（安卓控件变体）预期路径。

UI差异只能改变布局、密度、输入提示和表现，不能改变Gameplay权威结果。

真实需要验证：

- DPI Scaling（DPI缩放）
- Safe Zone（安全区域）
- 16:9、21:9及移动端纵横比
- Touch target（触控目标）尺寸
- Keyboard/Gamepad icon prompt（键盘/手柄图标提示）
- Mobile HUD density（移动HUD密度）

当前Android/PC Widget Blueprint均不存在，因此以上视觉验证状态为“未执行”。源码只提供平台Variant入口，不伪造测试通过。
