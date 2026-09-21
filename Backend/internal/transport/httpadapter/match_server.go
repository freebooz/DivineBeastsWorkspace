package httpadapter

import (
	"net/http"
	"sort"

	"divinebeasts/backend/internal/app/matchapi"
)

// NewMatchHandler（创建MatchService HTTP处理器）暴露Party和Matchmaking内部接口。
func NewMatchHandler(service *matchapi.Service) http.Handler {
	if service == nil {
		panic("Match API Service不能为空")
	}
	mux := http.NewServeMux()
	mux.HandleFunc("POST /internal/v1/match/party", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID    string `json:"playerId"`
			DisplayName string `json:"displayName"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		value, err := service.CreateParty(r.Context(), req.PlayerID, req.DisplayName)
		if err != nil {
			writeError(w, http.StatusBadRequest, "PARTY_CREATE_FAILED", err)
			return
		}
		members := make([]string, 0, len(value.Members))
		for playerID := range value.Members {
			members = append(members, playerID)
		}
		sort.Strings(members)
		writeJSON(w, http.StatusOK, map[string]any{"partyId": value.ID, "leaderPlayerId": value.LeaderPlayerID, "memberPlayerIds": members, "selectedArenaModeId": value.SelectedArenaModeID, "rosterLocked": value.RosterLocked, "revision": value.Revision})
	})
	mux.HandleFunc("POST /internal/v1/match/tickets", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			PlayerID        string `json:"playerId"`
			ArenaModeID     string `json:"arenaModeId"`
			PartyID         string `json:"partyId"`
			PreferredRegion string `json:"preferredRegion"`
			ClientRequestID string `json:"clientRequestId"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		ticket, err := service.CreateMatchmakingTicket(r.Context(), req.PlayerID, matchapi.CreateMatchmakingTicketInput{ArenaModeID: req.ArenaModeID, PartyID: req.PartyID, RegionID: req.PreferredRegion, ClientRequestID: req.ClientRequestID})
		if err != nil {
			writeError(w, http.StatusBadRequest, "MATCH_TICKET_CREATE_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"ticketId": ticket.TicketID, "arenaModeId": ticket.ArenaModeID, "partyId": ticket.PartyID, "partyMemberIds": ticket.PartyMemberIDs, "partySize": ticket.PartySize, "teamSize": ticket.TeamSize, "state": ticket.State})
	})
	return mux
}
