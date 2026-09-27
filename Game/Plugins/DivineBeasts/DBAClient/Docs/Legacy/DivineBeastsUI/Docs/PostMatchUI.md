# PostMatchUI（赛后结果界面）

UI明确区分：

- ResultPending：服务器已结束战斗，但结果尚在提交/确认。
- Committed：平台Arena phase进入Completed，结果已确认。
- Failed：Aborted/Failed。

ResultCommitState由复制的Arena MatchPhase映射，不由UI猜测。

UI只展示服务器同步结果，不计算：

- Winner
- Rating
- Rewards

Return World（返回世界）命令通过 DivineBeastsArenaClient 的 RequestPostMatchReturnToWorld 进入ApplicationFlow，再申请新的OpenWorld Assignment和TransferTicket；不能复用旧Endpoint或旧Ticket。
