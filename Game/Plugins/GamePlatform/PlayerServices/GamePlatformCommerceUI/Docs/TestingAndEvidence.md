# 测试与证据

更新日期：2026-09-30。真实源码范围为GamePlatformCommerceUI现存模块及Private/Tests；测试替身只控制完成时序，不作为生产后端。模块构建、UE测试运行、后端联调和Cook分开记录。

Task 2一次性源码回归位于Game/Saved/Reviews/task2-regression.py，初次12项失败(exit1)，修复后12项静态通过(exit0)。它只能检查危险源码路径已撤销，不能证明UE生命周期、资源释放或磁盘行为通过。Loading/Input现有原生策略套件Debug/Release实际执行，结果写Task 2报告；这些不是本插件业务或后端运行证据。

本次领域测试在Private/Tests中包含账号隔离及真实Online子系统的完成/错误/取消迟到响应/退出后弱引用释放用例；Equipment另有ASC持续存活的组件移除用例。UE5.8编译与实际Automation结果由统一执行账本记录，本页不预填通过数。

后端持久化、授权、数据库并发、消息/奖励链、真实支付、联机、干净Cook与人工体验本次未执行。旧VerifyEntitlement/旧后端目录或旧假支付用例的宣称已撤销；契约测试和接口代码不能替代相应运行实现。
