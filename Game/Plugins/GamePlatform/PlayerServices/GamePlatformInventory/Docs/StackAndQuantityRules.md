# StackAndQuantityRules（堆叠与数量规则）

Quantity（数量）在 Go/PostgreSQL 中使用整数；数据库约束 `quantity > 0`。Grant/Consume/Split 不接受 0 或负数。

Stackable（可堆叠）物品满足 `1 <= Quantity <= MaxStackSize`。Grant 优先填充现有兼容 Stack（堆叠），再创建新实例；如果容量不足，整个事务返回 InventoryFull（背包已满）并回滚，不留下半授予状态。

Unique（唯一实例型）策略要求 `MaxStackSize=1`且不可 Stackable；一次 Grant 的 Quantity 必须为 1。非 Stackable（不可堆叠）定义同样要求 MaxStackSize=1。
