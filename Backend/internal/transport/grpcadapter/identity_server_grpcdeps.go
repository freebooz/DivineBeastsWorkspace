//go:build grpcdeps

// Package grpcadapter（gRPC传输适配器）把生成的Protobuf RPC类型映射到稳定的Go应用/领域服务。
package grpcadapter

import (
	"context"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/platform/apperror"
	"errors"

	identityv1 "divinebeasts/backend/internal/generated/identity/v1"
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

// Login 生产使用密码领域用例；guest是否允许由Service构造决定，失败不回退。
func (s *IdentityServer) Login(ctx context.Context, req *identityv1.LoginRequest) (*identityv1.LoginResponse, error) {
	if req.GetProvider() != "guest" && req.GetProvider() != "password" {
		return &identityv1.LoginResponse{ErrorCode: "AUTH_PROVIDER_UNSUPPORTED"}, nil
	}
	var session identity.Session
	var err error
	if req.GetProvider() == "password" {
		session, err = s.service.LoginPassword(ctx, req.GetGameId(), req.GetAccountName(), req.GetCredential(), req.GetDeviceId())
	} else {
		session, err = s.service.LoginGuest(ctx, req.GetGameId(), req.GetDeviceId())
	}
	if err != nil {
		return &identityv1.LoginResponse{ErrorCode: onlineDomainCode(err)}, nil
	}
	return &identityv1.LoginResponse{
		PlayerId: session.PlayerID, SessionId: session.SessionID, AccessToken: session.AccessToken,
		RefreshToken: session.RefreshToken, ExpiresAtUnixMs: session.AccessExpiresAt.UnixMilli(),
		RefreshExpiresAtUnixMs: session.RefreshExpiresAt.UnixMilli(),
	}, nil
}

// Authenticate（Access Token认证）返回Gateway后续调用所需的可信PlayerID和SessionID。
func (s *IdentityServer) Authenticate(ctx context.Context, req *identityv1.AuthenticateRequest) (*identityv1.AuthenticateResponse, error) {
	session, err := s.service.Authenticate(ctx, req.GetAccessToken())
	if err != nil {
		return &identityv1.AuthenticateResponse{Valid: false, ErrorCode: onlineDomainCode(err)}, nil
	}
	return &identityv1.AuthenticateResponse{Valid: true, PlayerId: session.PlayerID, SessionId: session.SessionID}, nil
}

// Refresh 保留领域绝对到期与原子轮换语义，不在适配器重试。
func (s *IdentityServer) Refresh(ctx context.Context, req *identityv1.RefreshRequest) (*identityv1.LoginResponse, error) {
	session, err := s.service.Refresh(ctx, req.GetRefreshToken())
	if err != nil {
		return &identityv1.LoginResponse{ErrorCode: onlineDomainCode(err)}, nil
	}
	return &identityv1.LoginResponse{PlayerId: session.PlayerID, SessionId: session.SessionID, AccessToken: session.AccessToken, RefreshToken: session.RefreshToken, ExpiresAtUnixMs: session.AccessExpiresAt.UnixMilli(), RefreshExpiresAtUnixMs: session.RefreshExpiresAt.UnixMilli()}, nil
}
func (s *IdentityServer) Logout(ctx context.Context, req *identityv1.RefreshRequest) (*identityv1.LogoutResponse, error) {
	return &identityv1.LogoutResponse{ErrorCode: onlineDomainCode(s.service.Logout(ctx, req.GetRefreshToken()))}, nil
}
func (s *IdentityServer) Probe(ctx context.Context, _ *identityv1.ProbeRequest) (*identityv1.ProbeResponse, error) {
	err := s.service.Probe(ctx)
	return &identityv1.ProbeResponse{Ready: err == nil, ErrorCode: onlineDomainCode(err)}, nil
}

// onlineDomainCode 不向RPC响应复制SQL、连接地址或凭据；未知错误稳定为不可用。
func onlineDomainCode(err error) string {
	if err == nil {
		return ""
	}
	code := gateway.ServiceError("SERVICE_UNAVAILABLE")
	var app *apperror.Error
	if errors.As(err, &app) {
		code = gateway.ServiceError(app.Code)
	} else {
		switch {
		case errors.Is(err, identity.ErrInvalidCredentials):
			code = "AUTH_INVALID_CREDENTIALS"
		case errors.Is(err, identity.ErrInvalidToken):
			code = "AUTH_SESSION_INVALID"
		case errors.Is(err, identity.ErrTokenExpired):
			code = "AUTH_TOKEN_EXPIRED"
		case errors.Is(err, identity.ErrInvalidInput):
			code = "INVALID_REQUEST"
		}
	}
	_, safe := gateway.OnlineErrorStatus(code)
	return safe
}
