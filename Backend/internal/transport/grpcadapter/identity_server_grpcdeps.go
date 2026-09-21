//go:build grpcdeps

// Package grpcadapter（gRPC传输适配器）把生成的Protobuf RPC类型映射到稳定的Go应用/领域服务。
package grpcadapter

import (
	"context"
	"time"

	identityv1 "divinebeasts/backend/generated/proto/internal/identity/v1"
	"divinebeasts/backend/internal/modules/identity"
	"google.golang.org/grpc"
)

// IdentityServer（身份认证gRPC服务端）把内部Identity RPC映射到身份领域服务。
type IdentityServer struct {
	identityv1.UnimplementedIdentityServiceServer
	service *identity.Service
}

// RegisterIdentityServer（注册身份gRPC服务）把IdentityServer挂载到共享gRPC Server。
func RegisterIdentityServer(registrar grpc.ServiceRegistrar, service *identity.Service) {
	identityv1.RegisterIdentityServiceServer(registrar, &IdentityServer{service: service})
}

// Login（登录）当前第一版正式支持guest游客登录，其他Provider由后续平台Adapter扩展。
func (s *IdentityServer) Login(ctx context.Context, req *identityv1.LoginRequest) (*identityv1.LoginResponse, error) {
	if req.GetProvider() != "guest" {
		return &identityv1.LoginResponse{ErrorCode: "AUTH_PROVIDER_UNSUPPORTED"}, nil
	}
	session, err := s.service.LoginGuest(ctx, req.GetGameId(), req.GetDeviceId())
	if err != nil {
		return &identityv1.LoginResponse{ErrorCode: "AUTH_INVALID_CREDENTIALS"}, nil
	}
	return &identityv1.LoginResponse{
		PlayerId: session.PlayerID, SessionId: session.SessionID, AccessToken: session.AccessToken,
		RefreshToken: session.RefreshToken, ExpiresAtUnixMs: session.AccessExpiresAt.UnixMilli(),
	}, nil
}

// Authenticate（Access Token认证）返回Gateway后续调用所需的可信PlayerID和SessionID。
func (s *IdentityServer) Authenticate(ctx context.Context, req *identityv1.AuthenticateRequest) (*identityv1.AuthenticateResponse, error) {
	session, err := s.service.Authenticate(ctx, req.GetAccessToken())
	if err != nil {
		return &identityv1.AuthenticateResponse{Valid: false, ErrorCode: "AUTH_SESSION_INVALID"}, nil
	}
	if time.Now().UTC().After(session.AccessExpiresAt) {
		return &identityv1.AuthenticateResponse{Valid: false, ErrorCode: "AUTH_TOKEN_EXPIRED"}, nil
	}
	return &identityv1.AuthenticateResponse{Valid: true, PlayerId: session.PlayerID, SessionId: session.SessionID}, nil
}
