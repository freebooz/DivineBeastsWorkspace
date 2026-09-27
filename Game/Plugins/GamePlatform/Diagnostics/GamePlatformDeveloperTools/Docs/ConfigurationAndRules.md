# ConfigurationAndRules（配置与规则）

UGamePlatformValidationSettings（游戏平台验证设置）提供资产命名前缀、已废弃 GameplayTag 前缀、Server-safe 禁止依赖路径、Warning 阻断策略等 Editor 配置。

Build/Rules/developer-tools-validation.json 定义 PreSubmit/Nightly/Release Gate 规则集合。

Build/Rules/developer-tools-allowlist.json 保存临时豁免。每项必须具备 RuleId、Target、Reason、Owner、ApprovedBy、CreatedAt、ExpiresAt、Ticket；过期后重新失败。禁止 ignore_all。

平台层只提供通用默认值，MobaCommon/DivineBeasts 可通过扩展或配置单向增加项目规则。