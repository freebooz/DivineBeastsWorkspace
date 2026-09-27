# UIInputPolicy（UI输入策略）

`UGamePlatformUIInputPolicy`把平台枚举映射为 CommonUI `FUIInputConfig（输入配置）`：GameOnly→Game，UIOnly→Menu，GameAndUI→All。

UIOnly 会忽略移动和视角输入；GameOnly 保持游戏移动/视角；GameAndUI 不额外屏蔽。CommonUI 在 Activatable Widget 激活/反激活时负责应用和恢复配置。

Online/Dedicated（联网/专服）页面不能通过 UI 暂停服务器。PausePolicy 只有 `StandaloneOnly（仅单机）`，且仅 `NM_Standalone（单机网络模式）`允许调用 SetPause。

平台不创建第二套 Input Router（输入路由器）。GamePlatformInputClient 仍是正式输入能力 Owner，后续输入图标和设备提示从该插件接入。
