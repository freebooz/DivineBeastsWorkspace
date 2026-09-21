// Code generated from Shared/Contracts/GamePlatform. DO NOT EDIT.
// Package gameplatform（游戏平台共享Go绑定）保存公共契约版本和生成注册表。
package gameplatform

const ContractVersion = "1.3.0" // ContractVersion（公共跨语言契约版本）。

// OpenAPIOperations（OpenAPI OperationId（HTTP操作编号））。
var OpenAPIOperations = []string{
	"allocateMainArenaGameServer",
	"allocateWorldAndIssueTransfer",
	"allocateWorldGameServer",
	"authenticateIdentityInternal",
	"createGatewayMatchmakingTicket",
	"createGatewayParty",
	"createInternalMatchmakingTicket",
	"createInternalParty",
	"drainGameServer",
	"gatewayLogin",
	"getGameServerAssignment",
	"getGameServerControlLiveness",
	"getGameServerControlReadiness",
	"getGameServerControlVersion",
	"getGatewayLiveness",
	"getGatewayPlayerProfile",
	"getGatewayReadiness",
	"getGatewayVersion",
	"getIdentityLiveness",
	"getIdentityReadiness",
	"getIdentityVersion",
	"getInternalPlayerProfile",
	"getMatchmakingLiveness",
	"getMatchmakingReadiness",
	"getMatchmakingVersion",
	"getPartyLiveness",
	"getPartyReadiness",
	"getPartyVersion",
	"getPlayerDataLiveness",
	"getPlayerDataReadiness",
	"getPlayerDataVersion",
	"heartbeatGameServer",
	"issueServerTransferTicket",
	"loginIdentityInternal",
	"markGameServerReady",
	"registerGameServer",
	"submitAuthoritativeMatchResult",
	"validateServerTransferTicket",
}

// OpenAPIRoute（OpenAPI路由）由Shared契约生成，供Transport/Adapter避免手工复制HTTP路径。
type OpenAPIRoute struct{ OperationID, Method, Path string }

// OpenAPIRoutes（OpenAPI路由目录）保存OperationId、HTTP方法和路径。
var OpenAPIRoutes = []OpenAPIRoute{
	{OperationID: "allocateMainArenaGameServer", Method: "POST", Path: "/internal/v1/gameservers/allocate-mainarena"},
	{OperationID: "allocateWorldAndIssueTransfer", Method: "POST", Path: "/internal/v1/gameservers/allocate-world-transfer"},
	{OperationID: "allocateWorldGameServer", Method: "POST", Path: "/internal/v1/gameservers/allocate-world"},
	{OperationID: "authenticateIdentityInternal", Method: "POST", Path: "/internal/v1/identity/authenticate"},
	{OperationID: "createGatewayMatchmakingTicket", Method: "POST", Path: "/v1/matchmaking/tickets"},
	{OperationID: "createGatewayParty", Method: "POST", Path: "/v1/party"},
	{OperationID: "createInternalMatchmakingTicket", Method: "POST", Path: "/internal/v1/match/tickets"},
	{OperationID: "createInternalParty", Method: "POST", Path: "/internal/v1/match/party"},
	{OperationID: "drainGameServer", Method: "POST", Path: "/internal/v1/gameservers/drain"},
	{OperationID: "gatewayLogin", Method: "POST", Path: "/v1/auth/login"},
	{OperationID: "getGameServerAssignment", Method: "GET", Path: "/internal/v1/gameservers/assignment"},
	{OperationID: "getGameServerControlLiveness", Method: "GET", Path: "/health/live"},
	{OperationID: "getGameServerControlReadiness", Method: "GET", Path: "/health/ready"},
	{OperationID: "getGameServerControlVersion", Method: "GET", Path: "/version"},
	{OperationID: "getGatewayLiveness", Method: "GET", Path: "/health/live"},
	{OperationID: "getGatewayPlayerProfile", Method: "GET", Path: "/v1/player/profile"},
	{OperationID: "getGatewayReadiness", Method: "GET", Path: "/health/ready"},
	{OperationID: "getGatewayVersion", Method: "GET", Path: "/version"},
	{OperationID: "getIdentityLiveness", Method: "GET", Path: "/health/live"},
	{OperationID: "getIdentityReadiness", Method: "GET", Path: "/health/ready"},
	{OperationID: "getIdentityVersion", Method: "GET", Path: "/version"},
	{OperationID: "getInternalPlayerProfile", Method: "GET", Path: "/internal/v1/playerdata/profile"},
	{OperationID: "getMatchmakingLiveness", Method: "GET", Path: "/health/live"},
	{OperationID: "getMatchmakingReadiness", Method: "GET", Path: "/health/ready"},
	{OperationID: "getMatchmakingVersion", Method: "GET", Path: "/version"},
	{OperationID: "getPartyLiveness", Method: "GET", Path: "/health/live"},
	{OperationID: "getPartyReadiness", Method: "GET", Path: "/health/ready"},
	{OperationID: "getPartyVersion", Method: "GET", Path: "/version"},
	{OperationID: "getPlayerDataLiveness", Method: "GET", Path: "/health/live"},
	{OperationID: "getPlayerDataReadiness", Method: "GET", Path: "/health/ready"},
	{OperationID: "getPlayerDataVersion", Method: "GET", Path: "/version"},
	{OperationID: "heartbeatGameServer", Method: "POST", Path: "/internal/v1/gameservers/heartbeat"},
	{OperationID: "issueServerTransferTicket", Method: "POST", Path: "/internal/v1/gameservers/issue-transfer"},
	{OperationID: "loginIdentityInternal", Method: "POST", Path: "/internal/v1/identity/login"},
	{OperationID: "markGameServerReady", Method: "POST", Path: "/internal/v1/gameservers/ready"},
	{OperationID: "registerGameServer", Method: "POST", Path: "/internal/v1/gameservers/register"},
	{OperationID: "submitAuthoritativeMatchResult", Method: "POST", Path: "/internal/v1/gameservers/match-result"},
	{OperationID: "validateServerTransferTicket", Method: "POST", Path: "/internal/v1/gameservers/validate-transfer"},
}

// ProtoServices（Proto Services（跨语言RPC服务））。
var ProtoServices = []string{
	"GameServerControlService",
	"MatchResultService",
	"ServerTransferService",
}

// ProtoRPCs（Proto RPCs（跨语言RPC方法））。
var ProtoRPCs = []string{
	"GetGameServerAssignment",
	"IssueTransferTicket",
	"RegisterGameServer",
	"SetGameServerDraining",
	"SetGameServerReady",
	"SubmitMatchResult",
	"UpdateHeartbeat",
	"ValidateTransferTicket",
}
