//go:build grpcdeps

package grpcadapter

import (
	"context"

	matchv1 "divinebeasts/backend/internal/generated/match/v1"
	"divinebeasts/backend/internal/app/matchapi"
	"google.golang.org/grpc"
)

// MatchAPIServer（比赛业务gRPC服务端）提供Gateway所需的Party与Matchmaking入口。
type MatchAPIServer struct {
	matchv1.UnimplementedMatchServiceServer
	service *matchapi.Service
}

// RegisterMatchAPIServer（注册比赛业务gRPC服务）把MatchService接口挂载到gRPC Server。
func RegisterMatchAPIServer(registrar grpc.ServiceRegistrar, service *matchapi.Service) {
	matchv1.RegisterMatchServiceServer(registrar, &MatchAPIServer{service: service})
}

// CreateParty（创建Party）以请求玩家作为队长和首个成员。
func (s *MatchAPIServer) CreateParty(ctx context.Context, req *matchv1.CreatePartyRequest) (*matchv1.CreatePartyResponse, error) {
	value, err := s.service.CreateParty(ctx, req.GetPlayerId(), "")
	if err != nil {
		return &matchv1.CreatePartyResponse{ErrorCode: "PARTY_CREATE_FAILED"}, nil
	}
	members := make([]string, 0, len(value.Members))
	for playerID := range value.Members {
		members = append(members, playerID)
	}
	return &matchv1.CreatePartyResponse{
		PartyId: value.ID, LeaderPlayerId: value.LeaderPlayerID, MemberPlayerIds: members,
		SelectedArenaModeId: value.SelectedArenaModeID, RosterLocked: value.RosterLocked, Revision: value.Revision,
	}, nil
}

// CreateMatchmakingTicket（创建匹配票据）把Gateway请求映射到MatchService幂等应用接口。
func (s *MatchAPIServer) CreateMatchmakingTicket(ctx context.Context, req *matchv1.CreateMatchmakingTicketRequest) (*matchv1.CreateMatchmakingTicketResponse, error) {
	ticket, err := s.service.CreateMatchmakingTicket(ctx, req.GetPlayerId(), matchapi.CreateMatchmakingTicketInput{
		ArenaModeID: req.GetArenaModeId(), PartyID: req.GetPartyId(), RegionID: req.GetPreferredRegion(), ClientRequestID: req.GetClientRequestId(),
	})
	if err != nil {
		return &matchv1.CreateMatchmakingTicketResponse{ErrorCode: "MATCH_CREATE_TICKET_FAILED"}, nil
	}
	return &matchv1.CreateMatchmakingTicketResponse{
		TicketId: ticket.TicketID, ArenaModeId: ticket.ArenaModeID, PartyId: ticket.PartyID,
		PartyMemberIds: append([]string(nil), ticket.PartyMemberIDs...), PartySize: uint32(ticket.PartySize),
		TeamSize: uint32(ticket.TeamSize), State: ticket.State,
	}, nil
}
