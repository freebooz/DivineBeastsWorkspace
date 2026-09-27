# MovementAndCollision（移动与碰撞）

角色移动继续使用原生 UCharacterMovementComponent（角色移动组件），不创建自定义Movement Mode。

Definition控制的基础字段：CapsuleRadius(cm)、CapsuleHalfHeight(cm)、CollisionProfile、MaxWalkSpeed(cm/s)、MaxAcceleration(cm/s²)、JumpZVelocity(cm/s)、RotationRateYaw(deg/s)、bCanCrouch。

Definition验证要求尺寸/速度/加速度为有限合理值，CapsuleHalfHeight >= CapsuleRadius。

服务器可信Definition应用这些参数；客户端相同Definition用于网络预测一致性。客户端没有修改权威移动配置的RPC。
