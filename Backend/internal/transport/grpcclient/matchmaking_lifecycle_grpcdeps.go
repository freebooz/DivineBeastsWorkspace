//go:build grpcdeps

// 本文件复用已有Match gRPC连接；短命令5秒，事件长等待25秒，角色与会话由认证Gateway注入。
package grpcclient

import (
	"context"
	"divinebeasts/backend/internal/app/gateway"
	matchv1 "divinebeasts/backend/internal/generated/match/v1"
	"divinebeasts/backend/internal/modules/matchmaking"
	"encoding/json"
	"time"
)

// ReadMatchmakingState读取本人安全投影，错误不转换成idle。
func (c *MatchClient) ReadMatchmakingState(ctx context.Context, actor string) (matchmaking.PlayerSnapshot, error) {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	var result matchmaking.PlayerSnapshot
	response, err := c.client.ReadMatchmakingState(ctx, &matchv1.PartyReadRequest{PlayerId: actor})
	if err != nil {
		return result, err
	}
	err = decodePartyRPC(response, &result)
	return result, err
}

// ObserveMatchmaking保持可取消的有界等待，与普通命令使用不同预算。
func (c *MatchClient) ObserveMatchmaking(ctx context.Context, actor, cursor string, wait time.Duration) (matchmaking.Observation, error) {
	ctx, cancel := context.WithTimeout(ctx, 25*time.Second)
	defer cancel()
	var result matchmaking.Observation
	response, err := c.client.ObserveMatchmaking(ctx, &matchv1.PartyObservationRequest{PlayerId: actor, Cursor: cursor, WaitMilliseconds: wait.Milliseconds()})
	if err != nil {
		return result, err
	}
	err = decodePartyRPC(response, &result)
	return result, err
}

// matchCommandEnvelope只在内部传递可信上下文，不携带Token或密码。
func matchCommandEnvelope(actor matchmaking.ActorContext, command any) (*matchv1.MatchmakingCommandRequest, error) {
	a, err := json.Marshal(actor)
	if err != nil {
		return nil, err
	}
	b, err := json.Marshal(command)
	if err != nil {
		return nil, err
	}
	return &matchv1.MatchmakingCommandRequest{ActorJson: string(a), CommandJson: string(b)}, nil
}

// CommandMatchmaking保持原逻辑键和条件版本，不在传输层生成新报名。
func (c *MatchClient) CommandMatchmaking(ctx context.Context, actor matchmaking.ActorContext, command matchmaking.Command) (matchmaking.PlayerSnapshot, error) {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	var result matchmaking.PlayerSnapshot
	input, err := matchCommandEnvelope(actor, command)
	if err != nil {
		return result, err
	}
	response, err := c.client.CommandMatchmaking(ctx, input)
	if err != nil {
		return result, err
	}
	err = decodePartyRPC(response, &result)
	return result, err
}

// GetMatchmakingMigration只取本人秘密，调用方直接进入既有应用/Session流程。
func (c *MatchClient) GetMatchmakingMigration(ctx context.Context, actor matchmaking.ActorContext, command matchmaking.MigrationRequest) (matchmaking.Migration, error) {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	var result matchmaking.Migration
	input, err := matchCommandEnvelope(actor, command)
	if err != nil {
		return result, err
	}
	response, err := c.client.GetMatchmakingMigration(ctx, input)
	if err != nil {
		return result, err
	}
	err = decodePartyRPC(response, &result)
	return result, err
}

var _ gateway.MatchmakingLifecyclePort = (*MatchClient)(nil)
