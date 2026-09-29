package httpadapter

import (
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/modules/playerdata"
	"net/http"
	"strings"
)

// registerPlayerDataOnline 由领域用例独占写表，内部主体来自可信网关，不直接对公网开放。
func registerPlayerDataOnline(mux *http.ServeMux, s *playerdata.Service) {
	mux.HandleFunc("GET /internal/v1/playerdata/probe", func(w http.ResponseWriter, r *http.Request) {
		if err := s.Probe(r.Context()); err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeJSON(w, 200, map[string]bool{"ready": true})
	})
	mux.HandleFunc("POST /internal/v1/playerdata/ensure", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID string `json:"playerId"`
			GameID   string `json:"gameId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req, []string{"playerId", "gameId"}, []string{"playerId", "gameId"}); err != nil {
			writeError(w, 400, "INVALID_REQUEST", nil)
			return
		}
		if err := s.EnsureProfile(r.Context(), req.PlayerID, req.GameID); err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		w.WriteHeader(204)
	})
	mux.HandleFunc("PATCH /internal/v1/playerdata/profile", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID         string `json:"playerId"`
			DisplayName      string `json:"displayName"`
			ExpectedRevision int64  `json:"expectedRevision"`
			Key              string `json:"idempotencyKey"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req, []string{"playerId", "displayName", "expectedRevision", "idempotencyKey"}, []string{"playerId", "displayName", "expectedRevision", "idempotencyKey"}); err != nil {
			writeError(w, 400, "INVALID_REQUEST", nil)
			return
		}
		profile, err := s.UpdateDisplayNameIdempotent(r.Context(), req.PlayerID, req.DisplayName, req.ExpectedRevision, req.Key)
		if err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeInternalProfile(w, profile)
	})
	mux.HandleFunc("GET /internal/v1/playerdata/characters", func(w http.ResponseWriter, r *http.Request) {
		values := r.URL.Query()
		playerID := strings.TrimSpace(values.Get("playerId"))
		if playerID == "" || len(values) != 1 {
			writeError(w, 400, "INVALID_REQUEST", nil)
			return
		}
		characters, err := s.ListCharacters(r.Context(), playerID)
		if err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		if characters == nil {
			characters = []playerdata.Character{}
		}
		writeJSON(w, 200, map[string]any{"characters": characters})
	})
	mux.HandleFunc("POST /internal/v1/playerdata/characters", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID            string            `json:"playerId"`
			CreationRequestID   string            `json:"creationRequestId"`
			HeroDefinitionID    string            `json:"heroDefinitionId"`
			CharacterName       string            `json:"characterName"`
			AppearanceSelection map[string]string `json:"appearanceSelection"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req,
			[]string{"playerId", "creationRequestId", "heroDefinitionId", "characterName", "appearanceSelection"},
			[]string{"playerId", "creationRequestId", "heroDefinitionId", "characterName"}); err != nil {
			writeError(w, 400, "INVALID_REQUEST", nil)
			return
		}
		character, err := s.CreateCharacter(r.Context(), req.PlayerID, req.CreationRequestID, req.HeroDefinitionID, req.CharacterName, req.AppearanceSelection)
		if err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeJSON(w, 200, character)
	})
	mux.HandleFunc("POST /internal/v1/playerdata/character-selection", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID                  string `json:"playerId"`
			SelectionRequestID        string `json:"selectionRequestId"`
			CharacterID               string `json:"characterId"`
			ExpectedCharacterRevision int64  `json:"expectedCharacterRevision"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req,
			[]string{"playerId", "selectionRequestId", "characterId", "expectedCharacterRevision"},
			[]string{"playerId", "selectionRequestId", "characterId", "expectedCharacterRevision"}); err != nil {
			writeError(w, 400, "INVALID_REQUEST", nil)
			return
		}
		selection, err := s.SelectCharacter(r.Context(), req.PlayerID, req.SelectionRequestID, req.CharacterID, req.ExpectedCharacterRevision)
		if err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeJSON(w, 200, selection)
	})
}
func writeInternalProfile(w http.ResponseWriter, p playerdata.Profile) {
	if p.OwnedCharacterIDs == nil {
		p.OwnedCharacterIDs = []string{}
	}
	writeJSON(w, 200, map[string]any{"found": true, "playerId": p.PlayerID, "gameId": p.GameID, "displayName": p.DisplayName, "dataVersion": p.DataVersion, "revision": p.Revision, "tutorialCompleted": p.TutorialCompleted, "defaultWorldId": p.DefaultWorldID, "selectedCharacterId": p.SelectedCharacterID, "ownedCharacterIds": p.OwnedCharacterIDs})
}
