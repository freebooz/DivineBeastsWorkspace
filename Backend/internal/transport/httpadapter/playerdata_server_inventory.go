package httpadapter

import (
	"net/http"
	"strings"

	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/modules/inventory"
	"divinebeasts/backend/internal/modules/playerdata"
)

// registerPlayerDataInventory（注册PlayerData内部背包接口）只接受可信Gateway/内部服务传入的playerId。
// 公网客户端不能直接访问这些路由，所有身份归属必须在Gateway认证后确定。
func registerPlayerDataInventory(mux *http.ServeMux, service *playerdata.Service) {
	mux.HandleFunc("GET /internal/v1/playerdata/inventory", func(w http.ResponseWriter, r *http.Request) {
		playerID, ok := oneQueryValue(r, "playerId")
		if !ok {
			writeError(w, http.StatusBadRequest, "INVALID_REQUEST", nil)
			return
		}
		snapshot, err := service.GetInventorySnapshot(r.Context(), playerID)
		if err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, snapshot)
	})

	mux.HandleFunc("GET /internal/v1/playerdata/inventory/operations/{operationId}", func(w http.ResponseWriter, r *http.Request) {
		playerID, ok := oneQueryValue(r, "playerId")
		operationID := strings.TrimSpace(r.PathValue("operationId"))
		if !ok || operationID == "" {
			writeError(w, http.StatusBadRequest, "INVALID_REQUEST", nil)
			return
		}
		result, err := service.GetInventoryOperation(r.Context(), playerID, operationID)
		if err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, result)
	})

	mux.HandleFunc("POST /internal/v1/playerdata/inventory/move", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID          string `json:"playerId"`
			OperationID       string `json:"operationId"`
			ExpectedRevision  int64  `json:"expectedRevision"`
			ItemInstanceID    string `json:"itemInstanceId"`
			TargetContainerID string `json:"targetContainerId"`
			TargetSlotIndex   int32  `json:"targetSlotIndex"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req,
			[]string{"playerId", "operationId", "expectedRevision", "itemInstanceId", "targetContainerId", "targetSlotIndex"},
			[]string{"playerId", "operationId", "expectedRevision", "itemInstanceId", "targetContainerId", "targetSlotIndex"}); err != nil {
			writeError(w, http.StatusBadRequest, "INVALID_REQUEST", nil)
			return
		}
		result, err := service.MoveInventory(r.Context(), req.PlayerID, inventory.MoveCommand{
			OperationID: req.OperationID, ExpectedRevision: req.ExpectedRevision, ItemInstanceID: req.ItemInstanceID,
			TargetContainerID: req.TargetContainerID, TargetSlotIndex: req.TargetSlotIndex,
		})
		writeInventoryMutation(w, result, err)
	})

	mux.HandleFunc("POST /internal/v1/playerdata/inventory/split", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID             string `json:"playerId"`
			OperationID          string `json:"operationId"`
			ExpectedRevision     int64  `json:"expectedRevision"`
			SourceItemInstanceID string `json:"sourceItemInstanceId"`
			SplitQuantity        int32  `json:"splitQuantity"`
			TargetContainerID    string `json:"targetContainerId"`
			TargetSlotIndex      int32  `json:"targetSlotIndex"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req,
			[]string{"playerId", "operationId", "expectedRevision", "sourceItemInstanceId", "splitQuantity", "targetContainerId", "targetSlotIndex"},
			[]string{"playerId", "operationId", "expectedRevision", "sourceItemInstanceId", "splitQuantity", "targetContainerId", "targetSlotIndex"}); err != nil {
			writeError(w, http.StatusBadRequest, "INVALID_REQUEST", nil)
			return
		}
		result, err := service.SplitInventory(r.Context(), req.PlayerID, inventory.SplitCommand{
			OperationID: req.OperationID, ExpectedRevision: req.ExpectedRevision, SourceItemInstanceID: req.SourceItemInstanceID,
			SplitQuantity: req.SplitQuantity, TargetContainerID: req.TargetContainerID, TargetSlotIndex: req.TargetSlotIndex,
		})
		writeInventoryMutation(w, result, err)
	})

	mux.HandleFunc("POST /internal/v1/playerdata/inventory/merge", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID             string `json:"playerId"`
			OperationID          string `json:"operationId"`
			ExpectedRevision     int64  `json:"expectedRevision"`
			SourceItemInstanceID string `json:"sourceItemInstanceId"`
			TargetItemInstanceID string `json:"targetItemInstanceId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req,
			[]string{"playerId", "operationId", "expectedRevision", "sourceItemInstanceId", "targetItemInstanceId"},
			[]string{"playerId", "operationId", "expectedRevision", "sourceItemInstanceId", "targetItemInstanceId"}); err != nil {
			writeError(w, http.StatusBadRequest, "INVALID_REQUEST", nil)
			return
		}
		result, err := service.MergeInventory(r.Context(), req.PlayerID, inventory.MergeCommand{
			OperationID: req.OperationID, ExpectedRevision: req.ExpectedRevision,
			SourceItemInstanceID: req.SourceItemInstanceID, TargetItemInstanceID: req.TargetItemInstanceID,
		})
		writeInventoryMutation(w, result, err)
	})

	mux.HandleFunc("POST /internal/v1/playerdata/inventory/quickbar/set", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID         string `json:"playerId"`
			OperationID      string `json:"operationId"`
			ExpectedRevision int64  `json:"expectedRevision"`
			SlotIndex        int32  `json:"slotIndex"`
			ItemInstanceID   string `json:"itemInstanceId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req,
			[]string{"playerId", "operationId", "expectedRevision", "slotIndex", "itemInstanceId"},
			[]string{"playerId", "operationId", "expectedRevision", "slotIndex", "itemInstanceId"}); err != nil {
			writeError(w, http.StatusBadRequest, "INVALID_REQUEST", nil)
			return
		}
		result, err := service.SetInventoryQuickbar(r.Context(), req.PlayerID, inventory.QuickbarCommand{
			OperationID: req.OperationID, ExpectedRevision: req.ExpectedRevision,
			SlotIndex: req.SlotIndex, ItemInstanceID: req.ItemInstanceID,
		})
		writeInventoryMutation(w, result, err)
	})

	mux.HandleFunc("POST /internal/v1/playerdata/inventory/quickbar/clear", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID         string `json:"playerId"`
			OperationID      string `json:"operationId"`
			ExpectedRevision int64  `json:"expectedRevision"`
			SlotIndex        int32  `json:"slotIndex"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req,
			[]string{"playerId", "operationId", "expectedRevision", "slotIndex"},
			[]string{"playerId", "operationId", "expectedRevision", "slotIndex"}); err != nil {
			writeError(w, http.StatusBadRequest, "INVALID_REQUEST", nil)
			return
		}
		result, err := service.ClearInventoryQuickbar(r.Context(), req.PlayerID, inventory.QuickbarCommand{
			OperationID: req.OperationID, ExpectedRevision: req.ExpectedRevision, SlotIndex: req.SlotIndex,
		})
		writeInventoryMutation(w, result, err)
	})
}

func oneQueryValue(r *http.Request, name string) (string, bool) {
	values := r.URL.Query()
	value := strings.TrimSpace(values.Get(name))
	return value, value != "" && len(values) == 1
}

func writeInventoryMutation(w http.ResponseWriter, result inventory.MutationResult, err error) {
	if err != nil {
		writeOnlineDomainError(w, err)
		return
	}
	writeJSON(w, http.StatusOK, result)
}
