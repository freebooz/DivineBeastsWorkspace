# NetworkingAndAuthority（网络与权威）

最终 Health、Shield、Dead、Control 和 AvatarGeneration 由服务器权威产生。客户端通过属性复制、组件复制和 GAS GameplayEffect/Tag 观察状态。

`UGamePlatformAbilitySystemComponent`当前采用 Full GameplayEffect Replication，优先保证晚加入可恢复活动控制 Effect；后续可基于网络 Profile 再评估 Mixed/Minimal。

Combat 没有公开 `UFUNCTION(Server)`形式的任意伤害/Effect/SetHealth RPC。客户端只能通过未来受控 Ability/Target Intent 进入服务器可信路径。

双客户端 + Dedicated Server、Unauthorized 请求、晚加入、Avatar replacement 和延迟/丢包验证当前均未执行；不能由静态复制声明代替。
