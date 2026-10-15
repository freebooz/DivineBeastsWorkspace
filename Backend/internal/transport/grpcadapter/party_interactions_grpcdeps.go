//go:build grpcdeps

// 本文件将普通组队RPC适配到同一Match应用；JSON包围体复用Shared契约并执行相同严格校验。
// 不维护另一套组队规则，不把完整聚合或底层错误传入响应；仅可信内部Gateway可以声明PlayerID。
package grpcadapter

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"strings"
	"time"

	"divinebeasts/backend/internal/app/gateway"
	matchv1 "divinebeasts/backend/internal/generated/match/v1"
	"divinebeasts/backend/internal/modules/party"
)

// partyRPCResponse（安全编码）成功必须有可序列化授权快照；失败只返回稳定码，避免部分成功。
func partyRPCResponse(value any, err error) *matchv1.PartyDataResponse {
	if err != nil {
		return &matchv1.PartyDataResponse{ErrorCode: gateway.PartyErrorCode(err)}
	}
	payload, err := json.Marshal(value)
	if err != nil {
		return &matchv1.PartyDataResponse{ErrorCode: "SERVICE_UNAVAILABLE"}
	}
	return &matchv1.PartyDataResponse{PayloadJson: string(payload)}
}

// ReadPartyState（本人快照RPC）与内部HTTP调用同一授权投影，不直接返回仓储Party。
func (s *MatchAPIServer) ReadPartyState(ctx context.Context, request *matchv1.PartyReadRequest) (*matchv1.PartyDataResponse, error) {
	value, err := s.service.ReadPartyState(ctx, request.GetPlayerId())
	return partyRPCResponse(value, err), nil
}

// CommandParty（命令RPC）复用64KiB、字段白名单、重复键和尾随JSON保护，不信任Envelope中的任意actor字段。
func (s *MatchAPIServer) CommandParty(ctx context.Context, request *matchv1.PartyCommandRequest) (*matchv1.PartyDataResponse, error) {
	// 这里只构造解析输入，不启动HTTP请求或网络监听；命令真正执行仍由应用服务完成。
	input := &http.Request{Header: make(http.Header), Body: io.NopCloser(strings.NewReader(request.GetCommandJson()))}
	var command party.Command
	if err := gateway.DecodeOnlineJSON(input, &command, []string{"partyId", "action", "targetPlayerId", "membershipRequestId", "clientRequestId", "expectedRevision", "ready"}, []string{"partyId", "action", "clientRequestId", "expectedRevision"}); err != nil {
		return &matchv1.PartyDataResponse{ErrorCode: "INVALID_REQUEST"}, nil
	}
	value, err := s.service.CommandParty(ctx, request.GetPlayerId(), command)
	return partyRPCResponse(value, err), nil
}

// GetPartyInvitations（收件箱RPC）仅查询本人待处理邀请，时间由Match服务提供。
func (s *MatchAPIServer) GetPartyInvitations(ctx context.Context, request *matchv1.PartyReadRequest) (*matchv1.PartyDataResponse, error) {
	value, err := s.service.GetPartyInvitations(ctx, request.GetPlayerId())
	return partyRPCResponse(value, err), nil
}

// ObserveParty使用同一应用和Shared JSON合同；先验证毫秒范围再转换Duration，禁止溢出绕过20秒预算。
func(s *MatchAPIServer)ObserveParty(ctx context.Context,request *matchv1.PartyObservationRequest)(*matchv1.PartyDataResponse,error){
	if request.GetWaitMilliseconds()<0||request.GetWaitMilliseconds()>20000{return &matchv1.PartyDataResponse{ErrorCode:"INVALID_REQUEST"},nil}
	value,err:=s.service.ObserveParty(ctx,request.GetPlayerId(),request.GetCursor(),time.Duration(request.GetWaitMilliseconds())*time.Millisecond)
	return partyRPCResponse(value,err),nil
}
