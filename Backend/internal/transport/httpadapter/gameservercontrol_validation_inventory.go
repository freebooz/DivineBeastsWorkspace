package httpadapter

import (
	"net/http"
	"time"

	"divinebeasts/backend/internal/app/gameservercontrol"
)

type validationInventoryInstance struct {
	GameServerID    string    `json:"gameServerId"`
	ServerBootID    string    `json:"serverBootId"`
	ServerRoleID    string    `json:"serverRoleId"`
	ExperienceID    string    `json:"experienceId"`
	PublicEndpoint  string    `json:"publicEndpoint"`
	BuildVersion    string    `json:"buildVersion"`
	ProtocolVersion uint32    `json:"protocolVersion"`
	Status          string    `json:"status"`
	HeartbeatUTC    time.Time `json:"heartbeatUtc"`
}

func registerServerValidationInventory(mux *http.ServeMux, service *gameservercontrol.Service, bearer string) {
	mux.Handle("GET /internal/v1/gameservers/validation-inventory", requireGameServerControlBearer(bearer, func(w http.ResponseWriter, _ *http.Request) {
		instances := service.LocalValidationInstances()
		response := make([]validationInventoryInstance, 0, len(instances))
		for _, instance := range instances {
			response = append(response, validationInventoryInstance{
				GameServerID: instance.ID, ServerBootID: instance.ServerBootID, ServerRoleID: instance.RoleID,
				ExperienceID: instance.ExperienceID, PublicEndpoint: instance.PublicEndpoint,
				BuildVersion: instance.BuildVersion, ProtocolVersion: instance.ProtocolVersion, Status: string(instance.Status),
				HeartbeatUTC: instance.LastHeartbeat.UTC(),
			})
		}
		w.Header().Set("Cache-Control", "no-store")
		writeJSON(w, http.StatusOK, struct {
			Instances []validationInventoryInstance `json:"instances"`
		}{Instances: response})
	}))
}
