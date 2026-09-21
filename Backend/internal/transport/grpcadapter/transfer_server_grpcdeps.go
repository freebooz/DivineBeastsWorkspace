//go:build grpcdeps

package grpcadapter

import (
	"context"
	"net"
	"strconv"
	"time"

	transferv1 "divinebeasts/backend/generated/proto/shared/gameplatform/transfer/v1"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/modules/servertransfer"
	"google.golang.org/grpc"
)

// ServerTransferServer（服务器迁移gRPC服务端）负责Shared Contract中的迁移票据签发和验证。
type ServerTransferServer struct {
	transferv1.UnimplementedServerTransferServiceServer
	service *gameservercontrol.Service
	nextID  func(prefix string) string
	ttl     time.Duration
}

// RegisterServerTransferServer（注册服务器迁移RPC）注入Ticket ID生成器和统一TTL。
func RegisterServerTransferServer(registrar grpc.ServiceRegistrar, service *gameservercontrol.Service, nextID func(prefix string) string, ttl time.Duration) {
	transferv1.RegisterServerTransferServiceServer(registrar, &ServerTransferServer{service: service, nextID: nextID, ttl: ttl})
}

// IssueTransferTicket（签发迁移票据）返回完整签名上下文，供Game Client携带到目标Dedicated Server。
func (s *ServerTransferServer) IssueTransferTicket(ctx context.Context, req *transferv1.IssueTransferTicketRequest) (*transferv1.IssueTransferTicketResponse, error) {
	_ = ctx
	ticket, err := s.service.IssueTransfer(gameservercontrol.IssueTransferInput{
		TicketID: s.nextID("transfer"), AssignmentID: req.GetAssignmentId(), GameID: req.GetGameId(), PlayerID: req.GetPlayerId(), SessionID: req.GetSessionId(),
		SourceGameServerID: req.GetSourceGameServerId(), DestinationGameServerID: req.GetDestinationGameServerId(),
		DestinationWorldID: req.GetDestinationWorldId(), DestinationExperienceID: req.GetDestinationExperienceId(), MatchID: req.GetMatchId(), TTL: s.ttl,
	})
	if err != nil {
		return &transferv1.IssueTransferTicketResponse{ErrorCode: "TRANSFER_TICKET_ISSUE_FAILED"}, nil
	}
	host, portText, err := net.SplitHostPort(ticket.DestinationEndpoint)
	if err != nil {
		return &transferv1.IssueTransferTicketResponse{ErrorCode: "TRANSFER_DESTINATION_INVALID"}, nil
	}
	port, err := strconv.Atoi(portText)
	if err != nil || port <= 0 {
		return &transferv1.IssueTransferTicketResponse{ErrorCode: "TRANSFER_DESTINATION_INVALID"}, nil
	}
	return &transferv1.IssueTransferTicketResponse{
		TicketId: ticket.TicketID, DestinationHost: host, DestinationPort: uint32(port), ExpiresAtUnixMs: ticket.ExpiresAt.UnixMilli(),
		Nonce: ticket.Nonce, Signature: ticket.Signature, GameId: ticket.GameID, PlayerId: ticket.PlayerID, SessionId: ticket.SessionID,
		SourceGameServerId: ticket.SourceGameServerID, DestinationGameServerId: ticket.DestinationGameServerID,
		DestinationWorldId: ticket.DestinationWorldID, MatchId: ticket.MatchID, IssuedAtUnixMs: ticket.IssuedAt.UnixMilli(), AssignmentId: ticket.AssignmentID, DestinationExperienceId: ticket.DestinationExperienceID,
	}, nil
}

// ValidateTransferTicket（验证迁移票据）重建完整Ticket后校验签名、TTL、目标绑定和一次性使用约束。
func (s *ServerTransferServer) ValidateTransferTicket(ctx context.Context, req *transferv1.ValidateTransferTicketRequest) (*transferv1.ValidateTransferTicketResponse, error) {
	_ = ctx
	ticket := servertransfer.Ticket{
		TicketID: req.GetTicketId(), AssignmentID: req.GetAssignmentId(), GameID: req.GetGameId(), PlayerID: req.GetPlayerId(), SessionID: req.GetSessionId(),
		SourceGameServerID: req.GetSourceGameServerId(), DestinationGameServerID: req.GetDestinationGameServerId(),
		DestinationEndpoint: req.GetDestinationEndpoint(), DestinationWorldID: req.GetDestinationWorldId(), DestinationExperienceID: req.GetDestinationExperienceId(), MatchID: req.GetMatchId(),
		IssuedAt: time.UnixMilli(req.GetIssuedAtUnixMs()).UTC(), ExpiresAt: time.UnixMilli(req.GetExpiresAtUnixMs()).UTC(),
		Nonce: req.GetNonce(), Signature: req.GetSignature(),
	}
	result, err := s.service.ValidateTransferContext(ctx, ticket, req.GetDestinationGameServerId())
	if err != nil {
		return &transferv1.ValidateTransferTicketResponse{Valid: false, ErrorCode: "TRANSFER_TICKET_INVALID"}, nil
	}
	return &transferv1.ValidateTransferTicketResponse{Valid: true, PlayerId: result.PlayerID, SessionId: result.SessionID, MatchId: result.MatchID, AssignmentId: result.AssignmentID, DestinationWorldId: result.DestinationWorldID, DestinationExperienceId: result.DestinationExperienceID}, nil
}
