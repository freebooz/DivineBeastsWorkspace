//go:build grpcdeps

// Package grpcclient（gRPC客户端适配器）把生成的Backend内部gRPC Client映射为Gateway应用端口。
package grpcclient

import (
	"context"
	"errors"
	"time"

	identityv1 "divinebeasts/backend/generated/proto/internal/identity/v1"
	matchv1 "divinebeasts/backend/generated/proto/internal/match/v1"
	playerdatav1 "divinebeasts/backend/generated/proto/internal/playerdata/v1"
	"divinebeasts/backend/internal/app/gateway"
	"google.golang.org/grpc"
)

// IdentityClient（身份gRPC客户端适配器）实现Gateway IdentityPort（身份端口）。
type IdentityClient struct {
	client identityv1.IdentityServiceClient
}

// NewIdentityClient（创建身份gRPC客户端）基于共享ClientConn复用HTTP/2连接。
func NewIdentityClient(conn grpc.ClientConnInterface) *IdentityClient {
	return &IdentityClient{client: identityv1.NewIdentityServiceClient(conn)}
}

// Login（登录）调用IdentityService并把Protobuf响应转换为Gateway DTO。
func (c *IdentityClient) Login(ctx context.Context, req gateway.LoginRequest) (gateway.LoginResponse, error) {
	response, err := c.client.Login(ctx, &identityv1.LoginRequest{GameId: req.GameID, Provider: req.Provider, Credential: req.Credential, ClientVersion: req.ClientVersion, DeviceId: req.DeviceID})
	if err != nil {
		return gateway.LoginResponse{}, err
	}
	if response.GetErrorCode() != "" {
		return gateway.LoginResponse{}, errors.New(response.GetErrorCode())
	}
	return gateway.LoginResponse{PlayerID: response.GetPlayerId(), SessionID: response.GetSessionId(), AccessToken: response.GetAccessToken(), RefreshToken: response.GetRefreshToken(), ExpiresAt: time.UnixMilli(response.GetExpiresAtUnixMs()).UTC().Format(time.RFC3339Nano)}, nil
}

// Authenticate（认证Access Token）调用IdentityService解析可信玩家上下文。
func (c *IdentityClient) Authenticate(ctx context.Context, token string) (gateway.AuthenticatedSession, error) {
	response, err := c.client.Authenticate(ctx, &identityv1.AuthenticateRequest{AccessToken: token})
	if err != nil {
		return gateway.AuthenticatedSession{}, err
	}
	if !response.GetValid() {
		return gateway.AuthenticatedSession{}, gateway.ErrUnauthorized
	}
	return gateway.AuthenticatedSession{PlayerID: response.GetPlayerId(), SessionID: response.GetSessionId()}, nil
}

// PlayerDataClient（玩家数据gRPC客户端适配器）实现Gateway PlayerDataPort（玩家数据端口）。
type PlayerDataClient struct {
	client playerdatav1.PlayerDataServiceClient
}

// NewPlayerDataClient（创建玩家数据gRPC客户端）基于共享连接创建生成Stub。
func NewPlayerDataClient(conn grpc.ClientConnInterface) *PlayerDataClient {
	return &PlayerDataClient{client: playerdatav1.NewPlayerDataServiceClient(conn)}
}

// GetProfile（获取玩家资料）调用PlayerDataService并转换为Gateway DTO。
func (c *PlayerDataClient) GetProfile(ctx context.Context, playerID string) (gateway.PlayerProfile, error) {
	response, err := c.client.GetProfile(ctx, &playerdatav1.GetProfileRequest{PlayerId: playerID})
	if err != nil {
		return gateway.PlayerProfile{}, err
	}
	if !response.GetFound() {
		return gateway.PlayerProfile{}, errors.New(response.GetErrorCode())
	}
	return gateway.PlayerProfile{PlayerID: response.GetPlayerId(), GameID: response.GetGameId(), DisplayName: response.GetDisplayName(), DataVersion: int(response.GetDataVersion()), Revision: response.GetRevision(), TutorialCompleted: response.GetTutorialCompleted(), DefaultWorldID: response.GetDefaultWorldId(), OwnedCharacterIDs: append([]string(nil), response.GetOwnedCharacterIds()...)}, nil
}

// MatchClient（比赛业务gRPC客户端适配器）同时实现Gateway PartyPort和MatchmakingPort。
type MatchClient struct{ client matchv1.MatchServiceClient }

// NewMatchClient（创建比赛业务gRPC客户端）复用与MatchService的HTTP/2连接。
func NewMatchClient(conn grpc.ClientConnInterface) *MatchClient {
	return &MatchClient{client: matchv1.NewMatchServiceClient(conn)}
}

// CreateParty（创建Party）调用MatchService并返回只读Party快照。
func (c *MatchClient) CreateParty(ctx context.Context, playerID string) (gateway.PartySnapshot, error) {
	response, err := c.client.CreateParty(ctx, &matchv1.CreatePartyRequest{PlayerId: playerID})
	if err != nil {
		return gateway.PartySnapshot{}, err
	}
	if response.GetErrorCode() != "" {
		return gateway.PartySnapshot{}, errors.New(response.GetErrorCode())
	}
	return gateway.PartySnapshot{PartyID: response.GetPartyId(), LeaderPlayerID: response.GetLeaderPlayerId(), MemberPlayerIDs: append([]string(nil), response.GetMemberPlayerIds()...), SelectedArenaModeID: response.GetSelectedArenaModeId(), RosterLocked: response.GetRosterLocked(), Revision: response.GetRevision()}, nil
}

// CreateTicket（创建匹配票据）调用MatchService并转换为Gateway响应DTO。
func (c *MatchClient) CreateTicket(ctx context.Context, playerID string, req gateway.CreateMatchmakingTicketRequest) (gateway.MatchmakingTicketResponse, error) {
	response, err := c.client.CreateMatchmakingTicket(ctx, &matchv1.CreateMatchmakingTicketRequest{PlayerId: playerID, ArenaModeId: req.ArenaModeID, PartyId: req.PartyID, PreferredRegion: req.PreferredRegion, ClientRequestId: req.ClientRequestID})
	if err != nil {
		return gateway.MatchmakingTicketResponse{}, err
	}
	if response.GetErrorCode() != "" {
		return gateway.MatchmakingTicketResponse{}, errors.New(response.GetErrorCode())
	}
	return gateway.MatchmakingTicketResponse{TicketID: response.GetTicketId(), ArenaModeID: response.GetArenaModeId(), PartyID: response.GetPartyId(), PartyMemberIDs: append([]string(nil), response.GetPartyMemberIds()...), PartySize: int(response.GetPartySize()), TeamSize: int(response.GetTeamSize()), State: response.GetState()}, nil
}
