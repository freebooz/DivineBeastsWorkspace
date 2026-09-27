# ValidationFramework（验证框架）

统一 Rule 模型字段：RuleId、RuleVersion、Category、Severity、Description、Rationale、Scope、Enabled、CIBlocking、AutoFixPolicy、Owner。

统一 Result 模型字段：RuleId、Target、Status、Message、Evidence、SuggestedFix。

第一版 AutoFixPolicy 默认 Disabled。Validator 只给 SuggestedFix，不批量重命名、移动或删除资产。

资产规则复用 UE5.8 UEditorValidatorBase；架构、构建工件、Cook、性能等不能由 Data Validation 完整覆盖的规则使用静态验证器、Commandlet 和 PowerShell CI Gate。