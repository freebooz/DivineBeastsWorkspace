# ShieldBehavior（护盾行为）

Shield只消费已存在的护盾事实并表现持续、Hit、Break等阶段，不维护第二套耐久、免伤或数值状态。颜色、强度、冲击位置等参数必须由Definition Schema声明。

持续Shield实例由World Registry持有，Stop、Travel、World teardown统一清理。质量降级可减少粒子、灯光或材质成本，但不能改变权威持续时间与数值。