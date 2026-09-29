package gateway

import (
	"context"
	"net/http"
	"strconv"
	"strings"

	"divinebeasts/backend/internal/modules/inventory"
)

// InventoryPort（背包端口）由PlayerData客户端适配器实现；Gateway只传递已认证玩家身份。
type InventoryPort interface {
	GetInventorySnapshot(context.Context, string) (inventory.Snapshot, error)
	GetInventoryOperation(context.Context, string, string) (inventory.MutationResult, error)
	MoveInventory(context.Context, string, inventory.MoveCommand) (inventory.MutationResult, error)
	SplitInventory(context.Context, string, inventory.SplitCommand) (inventory.MutationResult, error)
	MergeInventory(context.Context, string, inventory.MergeCommand) (inventory.MutationResult, error)
	SetInventoryQuickbar(context.Context, string, inventory.QuickbarCommand) (inventory.MutationResult, error)
	ClearInventoryQuickbar(context.Context, string, inventory.QuickbarCommand) (inventory.MutationResult, error)
}

type inventoryItemResponse struct {
	ItemInstanceID   string `json:"itemInstanceId"`
	ItemDefinitionID string `json:"itemDefinitionId"`
	Quantity         int32  `json:"quantity"`
	ContainerID      string `json:"containerId"`
	SlotIndex        int32  `json:"slotIndex"`
	Revision         string `json:"revision"`
	InstanceState    string `json:"instanceState"`
	MaxStackSize     int32  `json:"maxStackSize"`
}

type inventoryQuickbarResponse struct {
	SlotIndex      int32  `json:"slotIndex"`
	ItemInstanceID string `json:"itemInstanceId"`
	Revision       string `json:"revision"`
}

type inventoryContainerResponse struct {
	ContainerID string `json:"containerId"`
	Capacity    int32  `json:"capacity"`
}

type inventorySnapshotResponse struct {
	InventoryRevision string                       `json:"inventoryRevision"`
	Containers        []inventoryContainerResponse `json:"containers"`
	Items             []inventoryItemResponse      `json:"items"`
	Quickbar          []inventoryQuickbarResponse  `json:"quickbar"`
}

type inventoryMutationResponse struct {
	OperationID       string                    `json:"operationId"`
	Snapshot          inventorySnapshotResponse `json:"snapshot"`
	MovedQuantity     int32                     `json:"movedQuantity"`
	RemainingQuantity int32                     `json:"remainingQuantity"`
	Duplicate         bool                      `json:"duplicate"`
}

func (a *api) registerInventoryRoutes() {
	a.mux.HandleFunc("GET /v1/inventory", a.requireAuth(a.getInventory))
	a.mux.HandleFunc("GET /v1/inventory/operations/{operationId}", a.requireAuth(a.getInventoryOperation))
	a.mux.HandleFunc("POST /v1/inventory/move", a.requireAuth(a.moveInventory))
	a.mux.HandleFunc("POST /v1/inventory/split", a.requireAuth(a.splitInventory))
	a.mux.HandleFunc("POST /v1/inventory/merge", a.requireAuth(a.mergeInventory))
	a.mux.HandleFunc("POST /v1/inventory/quickbar/set", a.requireAuth(a.setInventoryQuickbar))
	a.mux.HandleFunc("POST /v1/inventory/quickbar/clear", a.requireAuth(a.clearInventoryQuickbar))
}

func (a *api) getInventory(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if a.inventory == nil {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	snapshot, err := a.inventory.GetInventorySnapshot(r.Context(), session.PlayerID)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	writeJSON(w, http.StatusOK, inventorySnapshotWire(snapshot))
}

func (a *api) getInventoryOperation(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if a.inventory == nil {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	operationID := strings.TrimSpace(r.PathValue("operationId"))
	if operationID == "" || len(operationID) > 128 {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	result, err := a.inventory.GetInventoryOperation(r.Context(), session.PlayerID, operationID)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	writeJSON(w, http.StatusOK, inventoryMutationWire(result))
}

func (a *api) moveInventory(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	var req struct {
		OperationID       string `json:"operationId"`
		ExpectedRevision  string `json:"expectedRevision"`
		ItemInstanceID    string `json:"itemInstanceId"`
		TargetContainerID string `json:"targetContainerId"`
		TargetSlotIndex   int32  `json:"targetSlotIndex"`
	}
	if !a.decodeInventoryMutation(w, r, &req,
		[]string{"operationId", "expectedRevision", "itemInstanceId", "targetContainerId", "targetSlotIndex"}) {
		return
	}
	revision, ok := parseInventoryRevision(req.ExpectedRevision)
	if !ok {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	result, err := a.inventory.MoveInventory(r.Context(), session.PlayerID, inventory.MoveCommand{
		OperationID: req.OperationID, ExpectedRevision: revision, ItemInstanceID: req.ItemInstanceID,
		TargetContainerID: req.TargetContainerID, TargetSlotIndex: req.TargetSlotIndex,
	})
	a.writeInventoryMutation(w, result, err)
}

func (a *api) splitInventory(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	var req struct {
		OperationID          string `json:"operationId"`
		ExpectedRevision     string `json:"expectedRevision"`
		SourceItemInstanceID string `json:"sourceItemInstanceId"`
		SplitQuantity        int32  `json:"splitQuantity"`
		TargetContainerID    string `json:"targetContainerId"`
		TargetSlotIndex      int32  `json:"targetSlotIndex"`
	}
	if !a.decodeInventoryMutation(w, r, &req,
		[]string{"operationId", "expectedRevision", "sourceItemInstanceId", "splitQuantity", "targetContainerId", "targetSlotIndex"}) {
		return
	}
	revision, ok := parseInventoryRevision(req.ExpectedRevision)
	if !ok {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	result, err := a.inventory.SplitInventory(r.Context(), session.PlayerID, inventory.SplitCommand{
		OperationID: req.OperationID, ExpectedRevision: revision, SourceItemInstanceID: req.SourceItemInstanceID,
		SplitQuantity: req.SplitQuantity, TargetContainerID: req.TargetContainerID, TargetSlotIndex: req.TargetSlotIndex,
	})
	a.writeInventoryMutation(w, result, err)
}

func (a *api) mergeInventory(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	var req struct {
		OperationID          string `json:"operationId"`
		ExpectedRevision     string `json:"expectedRevision"`
		SourceItemInstanceID string `json:"sourceItemInstanceId"`
		TargetItemInstanceID string `json:"targetItemInstanceId"`
	}
	if !a.decodeInventoryMutation(w, r, &req,
		[]string{"operationId", "expectedRevision", "sourceItemInstanceId", "targetItemInstanceId"}) {
		return
	}
	revision, ok := parseInventoryRevision(req.ExpectedRevision)
	if !ok {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	result, err := a.inventory.MergeInventory(r.Context(), session.PlayerID, inventory.MergeCommand{
		OperationID: req.OperationID, ExpectedRevision: revision,
		SourceItemInstanceID: req.SourceItemInstanceID, TargetItemInstanceID: req.TargetItemInstanceID,
	})
	a.writeInventoryMutation(w, result, err)
}

func (a *api) setInventoryQuickbar(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	var req struct {
		OperationID      string `json:"operationId"`
		ExpectedRevision string `json:"expectedRevision"`
		SlotIndex        int32  `json:"slotIndex"`
		ItemInstanceID   string `json:"itemInstanceId"`
	}
	if !a.decodeInventoryMutation(w, r, &req,
		[]string{"operationId", "expectedRevision", "slotIndex", "itemInstanceId"}) {
		return
	}
	revision, ok := parseInventoryRevision(req.ExpectedRevision)
	if !ok {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	result, err := a.inventory.SetInventoryQuickbar(r.Context(), session.PlayerID, inventory.QuickbarCommand{
		OperationID: req.OperationID, ExpectedRevision: revision, SlotIndex: req.SlotIndex, ItemInstanceID: req.ItemInstanceID,
	})
	a.writeInventoryMutation(w, result, err)
}

func (a *api) clearInventoryQuickbar(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	var req struct {
		OperationID      string `json:"operationId"`
		ExpectedRevision string `json:"expectedRevision"`
		SlotIndex        int32  `json:"slotIndex"`
	}
	if !a.decodeInventoryMutation(w, r, &req,
		[]string{"operationId", "expectedRevision", "slotIndex"}) {
		return
	}
	revision, ok := parseInventoryRevision(req.ExpectedRevision)
	if !ok {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	result, err := a.inventory.ClearInventoryQuickbar(r.Context(), session.PlayerID, inventory.QuickbarCommand{
		OperationID: req.OperationID, ExpectedRevision: revision, SlotIndex: req.SlotIndex,
	})
	a.writeInventoryMutation(w, result, err)
}

func (a *api) decodeInventoryMutation(w http.ResponseWriter, r *http.Request, target any, fields []string) bool {
	if a.inventory == nil {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return false
	}
	if err := DecodeOnlineJSON(r, target, fields, fields); err != nil {
		writeOnlineError(w, err)
		return false
	}
	return true
}

func (a *api) writeInventoryMutation(w http.ResponseWriter, result inventory.MutationResult, err error) {
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	writeJSON(w, http.StatusOK, inventoryMutationWire(result))
}

func parseInventoryRevision(value string) (int64, bool) {
	if value == "" || value != strings.TrimSpace(value) {
		return 0, false
	}
	revision, err := strconv.ParseInt(value, 10, 64)
	return revision, err == nil && revision > 0
}

func inventorySnapshotWire(snapshot inventory.Snapshot) inventorySnapshotResponse {
	out := inventorySnapshotResponse{
		InventoryRevision: strconv.FormatInt(snapshot.InventoryRevision, 10),
		Containers: make([]inventoryContainerResponse, 0, len(snapshot.Containers)),
		Items: make([]inventoryItemResponse, 0, len(snapshot.Items)),
		Quickbar: make([]inventoryQuickbarResponse, 0, len(snapshot.Quickbar)),
	}
	for _, container := range snapshot.Containers {
		out.Containers = append(out.Containers, inventoryContainerResponse{ContainerID: container.ContainerID, Capacity: container.Capacity})
	}
	for _, item := range snapshot.Items {
		out.Items = append(out.Items, inventoryItemResponse{
			ItemInstanceID: item.ItemInstanceID, ItemDefinitionID: item.ItemDefinitionID, Quantity: item.Quantity,
			ContainerID: item.ContainerID, SlotIndex: item.SlotIndex, Revision: strconv.FormatInt(item.Revision, 10),
			InstanceState: item.InstanceState, MaxStackSize: item.MaxStackSize,
		})
	}
	for _, slot := range snapshot.Quickbar {
		out.Quickbar = append(out.Quickbar, inventoryQuickbarResponse{
			SlotIndex: slot.SlotIndex, ItemInstanceID: slot.ItemInstanceID, Revision: strconv.FormatInt(slot.Revision, 10),
		})
	}
	return out
}

func inventoryMutationWire(result inventory.MutationResult) inventoryMutationResponse {
	return inventoryMutationResponse{
		OperationID: result.OperationID, Snapshot: inventorySnapshotWire(result.Snapshot),
		MovedQuantity: result.MovedQuantity, RemainingQuantity: result.RemainingQuantity, Duplicate: result.Duplicate,
	}
}
