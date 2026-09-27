# CIQualityGates（持续集成质量门禁）

PreSubmit：Naming、Changed Assets、Dependency、Stable ID、Definition、GameplayTag。

Nightly：All Assets、Reference Graph、Asset Audit、Client/Server Leak、Cook Manifest、Performance Smoke。

Release：Shipping Cook、Client/Server Manifest、Debug/Developer Leak、Secret Scan、Required Evidence、Performance Baseline。

Build/Rules/developer-tools-validation.json 定义阶段规则集合。Release 中任何必需步骤未执行时通过非0退出码阻断，不得写成通过。