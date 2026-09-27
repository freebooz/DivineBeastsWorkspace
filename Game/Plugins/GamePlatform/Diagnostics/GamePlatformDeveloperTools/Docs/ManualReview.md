# ManualReview（人工审查）

当前状态：未执行。

AI/Codex 可以：
- 生成 Review Case 模板。
- 创建状态为 NotRun 的 Review Report。
- 整理自动化证据。

AI/Codex 不可以：
- 填写虚假 Reviewer。
- 自动把人工验收改为 Passed。
- 在没有截图、日志、录屏或人工记录时宣称人工验收完成。

正式 Release 前由真实 Reviewer 执行所需 Case，并通过 UGamePlatformReviewSubsystem 的显式人工确认路径记录结论。