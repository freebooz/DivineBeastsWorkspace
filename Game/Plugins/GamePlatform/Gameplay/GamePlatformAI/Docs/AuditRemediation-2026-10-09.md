# AI候选容量整改（2026-10-09）

服务器PruneCandidates原来每移除一个超限候选就扫描全表。现在先剔除无效/过期/不合格候选，再一次partial_sort堆选择保留最新K项，复杂度O(N log K)，随后统一解绑被移除Actor的OnDestroyed。相同感知时间按稳定EntityId显式排序，不依赖TMap扫描顺序。

规则无UObject访问或静态状态，容器/Actor委托所有权留在Controller。现有原生CMake新增一万候选保留16项、容量内/零容量与比较次数回归；Debug/Release均通过。该输入的比较次数不是通用帧时间、CPU或内存预算实测；真实AI/导航压力与Insights尚未执行。
