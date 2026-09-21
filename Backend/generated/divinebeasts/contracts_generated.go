// Code generated from Shared Contracts. DO NOT EDIT.
// Package divinebeasts（神兽联盟共享协议生成元数据）。
package divinebeasts

// OpenAPIOperations（OpenAPI OperationId（HTTP操作编号））。
var OpenAPIOperations = []string{
	"getDivineBeastsCatalog",
}

// OpenAPIRoute（OpenAPI路由）由Shared契约生成，供Transport/Adapter避免手工复制HTTP路径。
type OpenAPIRoute struct{ OperationID, Method, Path string }

// OpenAPIRoutes（OpenAPI路由目录）保存OperationId、HTTP方法和路径。
var OpenAPIRoutes = []OpenAPIRoute{
	{OperationID: "getDivineBeastsCatalog", Method: "GET", Path: "/v1/games/divine-beasts/catalog"},
}

// ProtoServices（Proto Services（项目RPC服务））。
var ProtoServices = []string{}

// ProtoRPCs（Proto RPCs（项目RPC方法））。
var ProtoRPCs = []string{}
