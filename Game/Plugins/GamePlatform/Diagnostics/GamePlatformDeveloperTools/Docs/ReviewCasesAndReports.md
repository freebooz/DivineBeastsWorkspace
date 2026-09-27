# ReviewCasesAndReports（人工验收用例与报告）

UGamePlatformReviewCase 字段：CaseId、Category、Description、Preconditions、Steps、ExpectedResult、RequiredEvidence，并提供完整性检查。

UGamePlatformReviewReport 字段：RunId、CaseId、Status、Reviewer、ReviewDate、Comment、Evidence、Metrics、BuildVersion、ContentRevision。

UGamePlatformReviewSubsystem 创建报告时固定为 NotRun。ApplyHumanDecision 要求显式人工确认、Reviewer 非空；Passed 还必须提供 Evidence。

AI/Codex 只能生成待人工模板，不自动代签 Passed。