# API（接口）

主要公共API包括：

FDivineBeastsProjectCatalog（项目核心目录）：查询GameId、ProjectId、全部ServerRoleId、ExperienceId、ArenaModeId，并校验允许值与映射。

FDivineBeastsProjectContext（项目上下文）：组合项目身份、ServerRole、Experience、可选ArenaMode/World/Map/Region/Match以及Environment、BuildVersion、ContractVersion，并执行组合有效性校验。

FDivineBeastsContractVersion（契约版本查询）：读取当前ContractVersion、CatalogVersion、GeneratedRevision和Client↔Server、Server↔Backend兼容区间。

公开API不暴露protobuf生成DTO、不发HTTP、不执行RPC、不携带Secret或后端Credential（凭据）。
