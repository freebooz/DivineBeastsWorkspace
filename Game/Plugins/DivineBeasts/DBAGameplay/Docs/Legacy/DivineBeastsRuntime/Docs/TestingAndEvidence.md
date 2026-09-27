# TestingAndEvidence（测试与证据）

源码包含DivineBeasts.Runtime.ProjectCatalog、ProjectContext、ContractVersion的UE Automation测试，但当前Runner没有UE5.8工具链，因此这些C++自动化测试未执行。

可执行的静态/生成测试包括：Shared contract source validation、deterministic catalog codegen、clean regenerate、C++/Go generated parity、External Build.cs静态门禁、三层依赖架构和DeveloperTools架构门禁。

Go generated test源码已生成，但当前Runner没有go命令，因此go test状态为“未执行”。

Client/Server/Editor Build、External真实链接、Client/Server Cook必须基于真实工具链/产物后再改变状态。
