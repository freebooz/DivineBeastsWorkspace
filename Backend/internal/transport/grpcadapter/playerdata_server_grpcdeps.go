//go:build grpcdeps

package grpcadapter

import (
	"context"

	playerdatav1 "divinebeasts/backend/internal/generated/playerdata/v1"
	"divinebeasts/backend/internal/modules/playerdata"
	"google.golang.org/grpc"
)

// PlayerDataServer（玩家数据gRPC服务端）把GetProfile RPC映射到PlayerData领域服务。
type PlayerDataServer struct {
	playerdatav1.UnimplementedPlayerDataServiceServer
	service *playerdata.Service
}

// RegisterPlayerDataServer（注册玩家数据gRPC服务）把服务挂载到共享gRPC Server。
func RegisterPlayerDataServer(registrar grpc.ServiceRegistrar, service *playerdata.Service) {
	playerdatav1.RegisterPlayerDataServiceServer(registrar, &PlayerDataServer{service: service})
}

// GetProfile（获取玩家资料）返回跨局长期数据，不包含实时Gameplay状态。
func (s *PlayerDataServer) GetProfile(ctx context.Context, req *playerdatav1.GetProfileRequest) (*playerdatav1.GetProfileResponse, error) {
	profile, err := s.service.GetProfile(ctx, req.GetPlayerId())
	if err != nil {
		return &playerdatav1.GetProfileResponse{Found: false, ErrorCode: onlineDomainCode(err)}, nil
	}
	return profileResponse(profile), nil
}

func profileResponse(profile playerdata.Profile) *playerdatav1.GetProfileResponse {
	return &playerdatav1.GetProfileResponse{
		Found: true, PlayerId: profile.PlayerID, GameId: profile.GameID, DisplayName: profile.DisplayName,
		DataVersion: int32(profile.DataVersion), Revision: profile.Revision, TutorialCompleted: profile.TutorialCompleted,
		DefaultWorldId: profile.DefaultWorldID, SelectedCharacterId: profile.SelectedCharacterID,
		OwnedCharacterIds: append([]string(nil), profile.OwnedCharacterIDs...),
	}
}

// ListCharacters（读取持久角色）只返回当前可信playerID的角色列表。
func (s *PlayerDataServer) ListCharacters(ctx context.Context, req *playerdatav1.ListCharactersRequest) (*playerdatav1.ListCharactersResponse, error) {
	characters, err := s.service.ListCharacters(ctx, req.GetPlayerId())
	if err != nil {
		return &playerdatav1.ListCharactersResponse{ErrorCode: onlineDomainCode(err)}, nil
	}
	items := make([]*playerdatav1.CharacterSummary, 0, len(characters))
	for _, character := range characters {
		items = append(items, characterSummaryResponse(character))
	}
	return &playerdatav1.ListCharactersResponse{Characters: items}, nil
}

// CreateCharacter（创建持久角色）把生成Proto请求转换到PlayerData领域服务。
func (s *PlayerDataServer) CreateCharacter(ctx context.Context, req *playerdatav1.CreateCharacterRequest) (*playerdatav1.CreateCharacterResponse, error) {
	character, err := s.service.CreateCharacter(
		ctx, req.GetPlayerId(), req.GetCreationRequestId(), req.GetHeroDefinitionId(),
		req.GetCharacterName(), req.GetAppearanceSelection())
	if err != nil {
		return &playerdatav1.CreateCharacterResponse{ErrorCode: onlineDomainCode(err)}, nil
	}
	return &playerdatav1.CreateCharacterResponse{Character: characterSummaryResponse(character)}, nil
}

// SelectCharacter（选择持久角色）只有领域/仓储权威验证成功后返回角色和新的Profile Revision。
func (s *PlayerDataServer) SelectCharacter(ctx context.Context, req *playerdatav1.SelectCharacterRequest) (*playerdatav1.SelectCharacterResponse, error) {
	selection, err := s.service.SelectCharacter(
		ctx, req.GetPlayerId(), req.GetSelectionRequestId(), req.GetCharacterId(), req.GetExpectedCharacterRevision())
	if err != nil {
		return &playerdatav1.SelectCharacterResponse{ErrorCode: onlineDomainCode(err)}, nil
	}
	return &playerdatav1.SelectCharacterResponse{
		SelectionRequestId: selection.SelectionRequestID,
		ProfileRevision:    selection.ProfileRevision,
		Character:          characterSummaryResponse(selection.Character),
	}, nil
}

func characterSummaryResponse(character playerdata.Character) *playerdatav1.CharacterSummary {
	return &playerdatav1.CharacterSummary{
		CharacterId:         character.CharacterID,
		HeroDefinitionId:    character.HeroDefinitionID,
		CharacterName:       character.CharacterName,
		CharacterRevision:   character.CharacterRevision,
		OnboardingState:     character.OnboardingState,
		Status:              character.Status,
		AppearanceProfileId: character.AppearanceProfileID,
	}
}

// UpdateProfile 要求proto optional presence，不能将漏填修订号当作0。
func (s *PlayerDataServer) UpdateProfile(ctx context.Context, req *playerdatav1.UpdateProfileRequest) (*playerdatav1.GetProfileResponse, error) {
	if req.ExpectedRevision == nil {
		return &playerdatav1.GetProfileResponse{ErrorCode: "INVALID_REQUEST"}, nil
	}
	p, err := s.service.UpdateDisplayNameIdempotent(ctx, req.GetPlayerId(), req.GetDisplayName(), req.GetExpectedRevision(), req.GetIdempotencyKey())
	if err != nil {
		return &playerdatav1.GetProfileResponse{ErrorCode: onlineDomainCode(err)}, nil
	}
	return profileResponse(p), nil
}
func (s *PlayerDataServer) EnsureProfile(ctx context.Context, req *playerdatav1.EnsureProfileRequest) (*playerdatav1.EnsureProfileResponse, error) {
	return &playerdatav1.EnsureProfileResponse{ErrorCode: onlineDomainCode(s.service.EnsureProfile(ctx, req.GetPlayerId(), req.GetGameId()))}, nil
}
func (s *PlayerDataServer) Probe(ctx context.Context, _ *playerdatav1.ProbeRequest) (*playerdatav1.ProbeResponse, error) {
	err := s.service.Probe(ctx)
	return &playerdatav1.ProbeResponse{Ready: err == nil, ErrorCode: onlineDomainCode(err)}, nil
}
