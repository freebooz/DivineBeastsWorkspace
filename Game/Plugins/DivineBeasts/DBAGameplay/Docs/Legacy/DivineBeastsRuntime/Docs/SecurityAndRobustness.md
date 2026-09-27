# SecurityAndRobustness（安全与健壮性）

Runtime没有Secret、Token、数据库地址、后端Credential或私钥字段；ProjectContext不承担认证。

External Build.cs对Generated include缺失、目标平台无映射、静态库缺失均使用BuildException快速失败，不静默降级。

Shared项目Catalog拒绝Lobby ServerRole和旧FiveCamp/Faction/KingSeal/Resonance/BreakElement项目身份；Element只按旧项目玩法语义审查，避免误删普通技术词。

生成代码通过clean regenerate SHA-256比较防止手工漂移。当前工作区不是Git仓库，因此无法用Git diff作为Generated未手改证据，使用确定性再生成结果替代。
