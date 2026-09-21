// Code generated from Shared/Contracts/GamePlatform. DO NOT EDIT.
// Package gameplatform（游戏平台共享Go绑定）保存公共契约版本和生成注册表。
package gameplatform

const ContractVersion = "1.3.0" // ContractVersion（公共跨语言契约版本）。

// OpenAPIOperations（OpenAPI OperationId（HTTP操作编号））。
var OpenAPIOperations = []string{
	"cancelMatchmakingTicket",
	"createMatchmakingTicket",
	"createParty",
	"getAuthSession",
	"getGatewayHealth",
	"getGatewayVersion",
	"getMatchmakingTicket",
	"getOwnedCharacters",
	"getParty",
	"getPlayerProfile",
	"getPlayerProgress",
	"invitePartyMember",
	"joinParty",
	"leaveParty",
	"login",
	"logout",
	"refreshAccessToken",
	"setPartyReadyState",
	"transferPartyLeadership",
	"updatePlayerSettings",
}

// OpenAPIRoute（OpenAPI路由）由Shared契约生成，供Transport/Adapter避免手工复制HTTP路径。
type OpenAPIRoute struct{ OperationID, Method, Path string }

// OpenAPIRoutes（OpenAPI路由目录）保存OperationId、HTTP方法和路径。
var OpenAPIRoutes = []OpenAPIRoute{
	{OperationID: "cancelMatchmakingTicket", Method: "DELETE", Path: "/v1/matchmaking/tickets/{ticketId}"},
	{OperationID: "createMatchmakingTicket", Method: "POST", Path: "/v1/matchmaking/tickets"},
	{OperationID: "createParty", Method: "POST", Path: "/v1/party"},
	{OperationID: "getAuthSession", Method: "GET", Path: "/v1/auth/session"},
	{OperationID: "getGatewayHealth", Method: "GET", Path: "/health/live"},
	{OperationID: "getGatewayVersion", Method: "GET", Path: "/version"},
	{OperationID: "getMatchmakingTicket", Method: "GET", Path: "/v1/matchmaking/tickets/{ticketId}"},
	{OperationID: "getOwnedCharacters", Method: "GET", Path: "/v1/player/characters"},
	{OperationID: "getParty", Method: "GET", Path: "/v1/party/{partyId}"},
	{OperationID: "getPlayerProfile", Method: "GET", Path: "/v1/player/profile"},
	{OperationID: "getPlayerProgress", Method: "GET", Path: "/v1/player/progress"},
	{OperationID: "invitePartyMember", Method: "POST", Path: "/v1/party/{partyId}/invite"},
	{OperationID: "joinParty", Method: "POST", Path: "/v1/party/{partyId}/join"},
	{OperationID: "leaveParty", Method: "POST", Path: "/v1/party/{partyId}/leave"},
	{OperationID: "login", Method: "POST", Path: "/v1/auth/login"},
	{OperationID: "logout", Method: "POST", Path: "/v1/auth/logout"},
	{OperationID: "refreshAccessToken", Method: "POST", Path: "/v1/auth/refresh"},
	{OperationID: "setPartyReadyState", Method: "POST", Path: "/v1/party/{partyId}/ready"},
	{OperationID: "transferPartyLeadership", Method: "POST", Path: "/v1/party/{partyId}/leader"},
	{OperationID: "updatePlayerSettings", Method: "PATCH", Path: "/v1/player/settings"},
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
