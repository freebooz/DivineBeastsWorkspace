# SecurityAndSafety（安全与防误操作）

第一版 AutoFix 默认 Disabled，不自动批量重命名、移动、删除或修改资产。

Build/Validation/ValidateSecrets.ps1 只扫描已知敏感文件名、Private Key Header 与 Client Artifact 中可疑配置字段。报告仅输出路径、字段名、类型和 masked 摘要，不输出 Secret 值。

所有报告路径限制在 Game/Saved/GamePlatformValidation。RunId 禁止 ..、斜杠和反斜杠逃逸。

DeveloperTools 不连接 Production DB，不修改 PlayerData/订单/奖励，不新增 Go API，不执行任意不可信 Shell 字符串。