# AuthorityAndCommandQuery（权威与命令查询边界）

UI只做两件事：

- Query（查询）：消费只读View State。
- Intent（意图）：通过Command Port请求业务Owner执行。

UI不得成为权威执行者，不允许：

- SetHealth
- GiveAbility
- CompleteQuest
- SetScore
- SetWinner
- SetTeam
- SetTutorialComplete
- 本地权威CreateCharacter

当前组合根还会校验ApplicationFlow AllowedActions（允许动作），例如登录、创建角色、持久角色选择和登出只有在业务Owner明确允许时才提交。

RequestWorld由ApplicationFlow自身再次验证Experience/Selection/Assignment条件；UI不会选择具体Shard、Server或Endpoint。
