# ServerValidation（服务器验证）

Begin RPC 到达服务器后重新验证：Interactor Authority、Gameplay Active、额外 Eligibility、Target引用/World、InstanceId、Generation、Revision、Option、Enabled/Harvest余量、服务器距离、服务器视线、目标 CanBegin、并发占用、请求去重和 Rate Limit。

服务器距离使用当前 Pawn/Owner 位置与 Target SceneComponent 当前 InteractionPoint，采用平方距离比较；客户端不上传最终距离。

要求 LOS 时服务器从 Player ViewPoint/Actor Eyes 发 ECC_Visibility Trace，视点偏离 Interactor Origin 超过配置上限会退回服务器当前 Origin。客户端不上传“已验证视线”。

Gameplay Active 缺失 Provider 或 Provider 返回 false 时拒绝 `InteractorNotActive`，不会默认放行。
