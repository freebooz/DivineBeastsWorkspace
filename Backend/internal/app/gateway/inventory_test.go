package gateway

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"divinebeasts/backend/internal/modules/inventory"
)

type inventoryPlayerFake struct {
	fakePlayerData
	lastPlayerID string
	lastMove     inventory.MoveCommand
}

func (f *inventoryPlayerFake) GetInventorySnapshot(_ context.Context, playerID string) (inventory.Snapshot, error) {
	f.lastPlayerID = playerID
	return inventory.Snapshot{
		InventoryRevision: 9007199254740993,
		Containers: []inventory.ContainerSnapshot{{ContainerID: "main", Capacity: 64}},
		Items: []inventory.ItemInstance{{
			ItemInstanceID: "item-1", ItemDefinitionID: "Item.Definition.Test",
			Quantity: 2, ContainerID: "main", SlotIndex: 0, Revision: 9007199254740993,
			InstanceState: "active", MaxStackSize: 20,
		}},
		Quickbar: []inventory.QuickbarSlot{{SlotIndex: 0, ItemInstanceID: "item-1", Revision: 9007199254740993}},
	}, nil
}

func (f *inventoryPlayerFake) GetInventoryOperation(_ context.Context, playerID, operationID string) (inventory.MutationResult, error) {
	snapshot, _ := f.GetInventorySnapshot(context.Background(), playerID)
	return inventory.MutationResult{OperationID: operationID, Snapshot: snapshot, Duplicate: true}, nil
}

func (f *inventoryPlayerFake) MoveInventory(_ context.Context, playerID string, command inventory.MoveCommand) (inventory.MutationResult, error) {
	f.lastPlayerID = playerID
	f.lastMove = command
	snapshot, _ := f.GetInventorySnapshot(context.Background(), playerID)
	snapshot.InventoryRevision++
	return inventory.MutationResult{OperationID: command.OperationID, Snapshot: snapshot, MovedQuantity: 2}, nil
}

func (f *inventoryPlayerFake) SplitInventory(_ context.Context, playerID string, command inventory.SplitCommand) (inventory.MutationResult, error) {
	snapshot, _ := f.GetInventorySnapshot(context.Background(), playerID)
	return inventory.MutationResult{OperationID: command.OperationID, Snapshot: snapshot}, nil
}

func (f *inventoryPlayerFake) MergeInventory(_ context.Context, playerID string, command inventory.MergeCommand) (inventory.MutationResult, error) {
	snapshot, _ := f.GetInventorySnapshot(context.Background(), playerID)
	return inventory.MutationResult{OperationID: command.OperationID, Snapshot: snapshot}, nil
}

func (f *inventoryPlayerFake) SetInventoryQuickbar(_ context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	snapshot, _ := f.GetInventorySnapshot(context.Background(), playerID)
	return inventory.MutationResult{OperationID: command.OperationID, Snapshot: snapshot}, nil
}

func (f *inventoryPlayerFake) ClearInventoryQuickbar(_ context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	snapshot, _ := f.GetInventorySnapshot(context.Background(), playerID)
	return inventory.MutationResult{OperationID: command.OperationID, Snapshot: snapshot}, nil
}

func TestInventorySnapshotUsesAuthenticatedPlayerAndStringRevision(t *testing.T) {
	playerData := &inventoryPlayerFake{}
	handler := NewAPI(
		Config{ContractVersion: "1.0.0"},
		fakeIdentity{},
		playerData,
		fakeParty{},
		fakeMatchmaking{},
	)

	req := httptest.NewRequest(http.MethodGet, "/v1/inventory", nil)
	req.Header.Set("Authorization", "Bearer access-1")
	recorder := httptest.NewRecorder()
	handler.ServeHTTP(recorder, req)

	if recorder.Code != http.StatusOK {
		t.Fatalf("Inventory状态码=%d body=%s", recorder.Code, recorder.Body.String())
	}
	if playerData.lastPlayerID != "player-1" {
		t.Fatalf("Inventory必须使用认证主体，实际=%q", playerData.lastPlayerID)
	}

	var body struct {
		InventoryRevision string `json:"inventoryRevision"`
		Items []struct {
			Revision string `json:"revision"`
		} `json:"items"`
	}
	if err := json.NewDecoder(recorder.Body).Decode(&body); err != nil {
		t.Fatal(err)
	}
	if body.InventoryRevision != "9007199254740993" ||
		len(body.Items) != 1 ||
		body.Items[0].Revision != "9007199254740993" {
		t.Fatalf("Revision必须无损字符串化，body=%+v", body)
	}
}

func TestInventoryMoveParsesStringRevisionAndRejectsUnauthenticated(t *testing.T) {
	playerData := &inventoryPlayerFake{}
	handler := NewAPI(
		Config{},
		fakeIdentity{},
		playerData,
		fakeParty{},
		fakeMatchmaking{},
	)

	const body = "{\"operationId\":\"11111111-1111-1111-1111-111111111111\",\"expectedRevision\":\"9007199254740993\",\"itemInstanceId\":\"item-1\",\"targetContainerId\":\"main\",\"targetSlotIndex\":2}"
	req := httptest.NewRequest(http.MethodPost, "/v1/inventory/move", strings.NewReader(body))
	req.Header.Set("Authorization", "Bearer access-1")
	req.Header.Set("Content-Type", "application/json")
	recorder := httptest.NewRecorder()
	handler.ServeHTTP(recorder, req)

	if recorder.Code != http.StatusOK {
		t.Fatalf("Move状态码=%d body=%s", recorder.Code, recorder.Body.String())
	}
	if playerData.lastPlayerID != "player-1" ||
		playerData.lastMove.ExpectedRevision != 9007199254740993 {
		t.Fatalf("认证主体/Revision透传错误: player=%q move=%+v", playerData.lastPlayerID, playerData.lastMove)
	}

	unauthorized := httptest.NewRequest(http.MethodGet, "/v1/inventory", nil)
	unauthorized.Header.Set("Authorization", "Bearer invalid")
	unauthorizedRecorder := httptest.NewRecorder()
	handler.ServeHTTP(unauthorizedRecorder, unauthorized)
	if unauthorizedRecorder.Code != http.StatusUnauthorized {
		t.Fatalf("无效Token必须返回401，实际=%d", unauthorizedRecorder.Code)
	}
}
