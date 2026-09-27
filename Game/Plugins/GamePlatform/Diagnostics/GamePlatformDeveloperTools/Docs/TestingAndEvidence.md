# TestingAndEvidence（测试与证据）

C++ Automation Tests（自动化测试）覆盖：内置规则、Allowlist 有效/过期、旧 GameplayTag 拒绝、路径逃逸、Secret 脱敏、Review NotRun/禁止自动 Passed、性能基线兼容性。

Tests/Editor/DeveloperTools/TestDeveloperTools.ps1 覆盖 Editor-only 描述、Target 门禁、关键类/规则、脚本、26类文档和三层架构静态验证。

脚本输出明确记录 ueBuildVerified、editorAutomationVerified、dataValidationCommandletVerified、clientCookVerified、serverCookVerified、shippingVerified、benchmark10kVerified。

这些字段为 false 时不得在最终报告中改写为通过。