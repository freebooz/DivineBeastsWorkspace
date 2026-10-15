//go:build grpcdeps

// 本文件实现Gateway普通队伍端口的gRPC传输；仅连接现有MatchService，使用同一Shared JSON载荷和有限截止时间。
// 返回错误白名单由Gateway统一处理；不在传输层伪造无队伍、成员或邀请。
package grpcclient

import (
	"context"
	"encoding/json"
	"time"

	"divinebeasts/backend/internal/app/gateway"
	matchv1 "divinebeasts/backend/internal/generated/match/v1"
	"divinebeasts/backend/internal/modules/party"
)

// decodePartyRPC（解码成功载荷）错误与JSON同时存在、空成功或超限都视为下游故障。
func decodePartyRPC(response *matchv1.PartyDataResponse, target any) error {
	if response == nil {
		return gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	if response.GetErrorCode() != "" {
		if response.GetPayloadJson() != "" {
			return gateway.ServiceError("SERVICE_UNAVAILABLE")
		}
		return gateway.ServiceError(response.GetErrorCode())
	}
	if response.GetPayloadJson() == "" || len(response.GetPayloadJson()) > 65536 {
		return gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	if err := json.Unmarshal([]byte(response.GetPayloadJson()), target); err != nil {
		return gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return nil
}

// ReadPartyState（本人队伍）复用内部gRPC连接，身份只由认证后Gateway传入。
func (c *MatchClient) ReadPartyState(ctx context.Context, playerID string) (party.Snapshot, error) {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	var value party.Snapshot
	response, err := c.client.ReadPartyState(ctx, &matchv1.PartyReadRequest{PlayerId: playerID})
	if err != nil {
		return value, err
	}
	err = decodePartyRPC(response, &value)
	return value, err
}

// CommandParty（组队命令）按契约编码，不在gRPC侧改变幂等键或期望版本。
func (c *MatchClient) CommandParty(ctx context.Context, playerID string, command party.Command) (party.CommandResult, error) {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	var value party.CommandResult
	payload, err := json.Marshal(command)
	if err != nil {
		return value, err
	}
	response, err := c.client.CommandParty(ctx, &matchv1.PartyCommandRequest{PlayerId: playerID, CommandJson: string(payload)})
	if err != nil {
		return value, err
	}
	err = decodePartyRPC(response, &value)
	return value, err
}

// GetPartyInvitations（本人邀请）没有邀请返回真实空数组；下游故障不转换成空成功。
func (c *MatchClient) GetPartyInvitations(ctx context.Context, playerID string) ([]party.InvitationSnapshot, error) {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	value := []party.InvitationSnapshot{}
	response, err := c.client.GetPartyInvitations(ctx, &matchv1.PartyReadRequest{PlayerId: playerID})
	if err != nil {
		return nil, err
	}
	err = decodePartyRPC(response, &value)
	return value, err
}

var _ gateway.PartyInteractionPort = (*MatchClient)(nil)

// ObserveParty单次最长25秒；Context退出会取消服务端等待，普通命令的5秒预算不变。
func(c *MatchClient)ObserveParty(ctx context.Context,actor,cursor string,wait time.Duration)(party.Observation,error){
	ctx,cancel:=context.WithTimeout(ctx,25*time.Second);defer cancel()
	var result party.Observation
	response,err:=c.client.ObserveParty(ctx,&matchv1.PartyObservationRequest{PlayerId:actor,Cursor:cursor,WaitMilliseconds:wait.Milliseconds()})
	if err!=nil{return result,err};err=decodePartyRPC(response,&result);return result,err
}

var _ gateway.PartyObservationPort=(*MatchClient)(nil)
