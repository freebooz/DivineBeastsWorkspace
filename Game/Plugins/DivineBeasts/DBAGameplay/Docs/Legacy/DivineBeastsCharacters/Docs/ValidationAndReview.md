# ValidationAndReview（验证与审查）

DeveloperTools现有通用验证能力被复用：DefinitionId/Version、Stable ID重复、PrimaryAssetId、RequiredDefinitions循环、GameplayTag旧系统前缀、Server Asset Safety、项目资产依赖和Cook门禁。

项目特有“恰好12个生肖Hero、具体Stable ID、无MOBA依赖、Creation Provider边界、Generation防旧回调”由Build/Validation/VerifyDivineBeastsCharacters.ps1和Characters Integration脚本检查，避免基础层DeveloperTools反向依赖项目代码。

真实Data Validation、Asset Audit、Cook报告和Manual Review需要UE5.8环境；当前均未执行。
