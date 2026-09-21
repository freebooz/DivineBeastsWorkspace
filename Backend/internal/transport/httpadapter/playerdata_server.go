package httpadapter

import (
	"net/http"

	"divinebeasts/backend/internal/modules/playerdata"
)

// NewPlayerDataHandler（创建玩家数据HTTP处理器）暴露内部长期资料读取接口。
func NewPlayerDataHandler(service *playerdata.Service) http.Handler {
	if service == nil {
		panic("PlayerData Service不能为空")
	}
	mux := http.NewServeMux()
	mux.HandleFunc("GET /internal/v1/playerdata/profile", func(w http.ResponseWriter, r *http.Request) {
		playerID := r.URL.Query().Get("playerId")
		profile, err := service.GetProfile(r.Context(), playerID)
		if err != nil {
			writeError(w, http.StatusNotFound, "PLAYER_PROFILE_NOT_FOUND", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"found": true, "playerId": profile.PlayerID, "gameId": profile.GameID, "displayName": profile.DisplayName, "dataVersion": profile.DataVersion, "revision": profile.Revision, "tutorialCompleted": profile.TutorialCompleted, "defaultWorldId": profile.DefaultWorldID, "ownedCharacterIds": profile.OwnedCharacterIDs})
	})
	return mux
}
