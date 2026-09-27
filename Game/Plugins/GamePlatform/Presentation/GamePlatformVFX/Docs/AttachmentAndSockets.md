# AttachmentAndSockets（附着与插槽）

Attached、Trail、Shield及部分Beam通过SpawnContext AttachTarget/SocketName附着。附着只控制视觉Transform和生命周期，不拥有装备、攻击窗口、命中或动画权威。

目标失效时由Niagara生命周期或Service停止策略结束；平台不硬编码生肖、武器、英雄专属Socket。