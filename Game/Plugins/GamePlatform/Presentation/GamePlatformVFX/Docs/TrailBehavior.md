# TrailBehavior（拖尾行为）

Trail可附着Weapon、Character或视觉Projectile。它只处理视觉附着和生命周期，不拥有Attack Window（攻击窗口）、命中判定或伤害。

Cancel、死亡、换World和Travel时统一通过父句柄或WorldSubsystem清理。Pooling采用Niagara原生组件池，重新激活成本需要在真实性能测试中测量。