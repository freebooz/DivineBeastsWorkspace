// 本文件统一可信编排HTTP/gRPC的新增控制命令。只允许白名单操作，禁止反射调用任意服务方法。
package gameservercontrol

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"io"
	"strings"
)

// DecodeRuntimeInput限制正文大小、未知字段和尾随数据；仅用于已认证的后端内部边界。
func DecodeRuntimeInput(payload string, target any) error {
	if len(payload) == 0 || len(payload) > 65536 {
		return errors.New("INVALID_REQUEST: 控制正文大小无效")
	}
	// 先逐Token检查全部对象中的重复键，避免后值覆盖经过授权检查的前值。
	scan := json.NewDecoder(strings.NewReader(payload))
	var walk func() error
	walk = func() error {
		token, err := scan.Token()
		if err != nil {
			return err
		}
		if delimiter, ok := token.(json.Delim); ok {
			switch delimiter {
			case '{':
				seen := map[string]bool{}
				for scan.More() {
					key, err := scan.Token()
					if err != nil {
						return err
					}
					name, ok := key.(string)
					if !ok || seen[name] {
						return errors.New("INVALID_REQUEST: 重复字段")
					}
					seen[name] = true
					if err = walk(); err != nil {
						return err
					}
				}
				_, err = scan.Token()
				return err
			case '[':
				for scan.More() {
					if err = walk(); err != nil {
						return err
					}
				}
				_, err = scan.Token()
				return err
			default:
				return errors.New("INVALID_REQUEST: 无效JSON")
			}
		}
		return nil
	}
	if err := walk(); err != nil {
		return errors.New("INVALID_REQUEST: 控制正文无效")
	}
	if _, err := scan.Token(); err != io.EOF {
		return errors.New("INVALID_REQUEST: 尾随正文")
	}
	decoder := json.NewDecoder(bytes.NewBufferString(payload))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(target); err != nil {
		return errors.New("INVALID_REQUEST: 控制字段无效")
	}
	return nil
}

// ExecuteRuntimeCommand只供验证Bearer后的内部适配器调用，不接受玩家提供的actor或目标地址。
func (s *Service) ExecuteRuntimeCommand(ctx context.Context, action, payload string) (any, error) {
	switch action {
	case "allocate":
		var input AllocateMainArenaInput
		if err := DecodeRuntimeInput(payload, &input); err != nil {
			return nil, err
		}
		return s.AllocateMainArenaContext(ctx, input)
	case "issue":
		var input IssueTransferInput
		if err := DecodeRuntimeInput(payload, &input); err != nil {
			return nil, err
		}
		return s.IssueTransferContext(ctx, input)
	case "worldTransfer":
		var input AllocateWorldTransferInput
		if err := DecodeRuntimeInput(payload, &input); err != nil {
			return nil, err
		}
		return s.AllocateWorldTransfer(ctx, input)
	case "status", "cancel", "finalize":
		var input struct {
			MatchID string `json:"matchId"`
		}
		if err := DecodeRuntimeInput(payload, &input); err != nil || input.MatchID == "" || len(input.MatchID) > 128 {
			return nil, errors.New("INVALID_REQUEST: 比赛身份无效")
		}
		if action == "status" {
			return s.GetArenaStatus(ctx, input.MatchID)
		}
		var err error
		if action == "cancel" {
			err = s.CancelArena(ctx, input.MatchID)
		} else {
			err = s.FinalizeArena(ctx, input.MatchID)
		}
		return struct {
			Accepted bool `json:"accepted"`
		}{err == nil}, err
	case "revoke":
		var input struct {
			TicketID string `json:"ticketId"`
		}
		if err := DecodeRuntimeInput(payload, &input); err != nil || input.TicketID == "" || len(input.TicketID) > 128 {
			return nil, errors.New("INVALID_REQUEST: 签票身份无效")
		}
		err := s.RevokeTransfer(ctx, input.TicketID)
		return struct {
			Accepted bool `json:"accepted"`
		}{err == nil}, err
	case "presence":
		var input struct {
			PlayerID  string `json:"playerId"`
			SessionID string `json:"sessionId"`
		}
		if err := DecodeRuntimeInput(payload, &input); err != nil || input.PlayerID == "" || input.SessionID == "" {
			return nil, errors.New("INVALID_REQUEST: 本人位置身份无效")
		}
		return s.LookupPlayerPresence(ctx, input.PlayerID, input.SessionID)
	default:
		return nil, errors.New("INVALID_REQUEST: 不允许的生命周期动作")
	}
}
