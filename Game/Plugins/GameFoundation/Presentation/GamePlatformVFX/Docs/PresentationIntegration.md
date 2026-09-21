# PresentationIntegration（表现层集成）

目标架构：

`Gameplay/GAS → GamePlatformPresentation → GamePlatformVFXPresentationProvider → IGamePlatformVFXService`

当前包没有真实 `GamePlatformPresentation` 头文件，因此没有虚构其接口。

实际接入时：

1. 打开 `Private/Integration/Presentation/GamePlatformVFXPresentationProvider.*`。
2. 实现真实 Provider Interface。
3. 把中立表现请求转换为 `FGamePlatformVFXRequest`。
4. Provider 生命周期按 GamePlatformPresentation 真实注册/注销机制接入。

不要让 Gameplay 直接硬编码 Niagara 资产路径。
