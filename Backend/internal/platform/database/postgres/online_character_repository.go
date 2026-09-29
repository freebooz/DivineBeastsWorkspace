//go:build productiondeps

package postgres

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/json"
	"errors"

	"github.com/jackc/pgx/v5"

	"divinebeasts/backend/internal/modules/playerdata"
	"divinebeasts/backend/internal/platform/apperror"
)

const (
	createCharacterOperation = "CreateCharacter"
	selectCharacterOperation = "SelectCharacter"
)

// 编译期约束：生产玩家仓储必须实现持久角色端口，防止生产装配悄悄回退为不可用。
var _ playerdata.CharacterRepository = (*OnlinePlayerRepository)(nil)

// ListCharacters（读取持久角色列表）只返回指定玩家拥有的角色，按创建时间和ID稳定排序。
func (r *OnlinePlayerRepository) ListCharacters(ctx context.Context, playerID string) ([]playerdata.Character, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if !validOnlinePlayerIdentity(playerID) {
		return nil, onlinePlayerInvalid()
	}
	if r == nil || !r.PlayerRepository.onlinePlayerAvailable() {
		return nil, onlinePlayerUnavailable()
	}
	var exists bool
	if err := r.pool.inner.QueryRow(ctx, `SELECT EXISTS(SELECT 1 FROM player_profiles WHERE player_id=$1)`, playerID).Scan(&exists); err != nil {
		return nil, onlinePlayerError(err)
	}
	if !exists {
		return nil, apperror.New("PLAYER_PROFILE_NOT_FOUND", "玩家资料不存在", false)
	}

	rows, err := r.pool.inner.Query(ctx, `SELECT
		character_id,player_id,hero_definition_id,character_name,character_revision,
		onboarding_state,status,appearance_profile_id,appearance_selection
		FROM player_characters
		WHERE player_id=$1
		ORDER BY created_at,character_id`, playerID)
	if err != nil {
		return nil, onlinePlayerError(err)
	}
	defer rows.Close()

	result := make([]playerdata.Character, 0)
	for rows.Next() {
		character, err := scanOnlineCharacter(rows)
		if err != nil {
			return nil, onlinePlayerError(err)
		}
		result = append(result, character)
	}
	if err := rows.Err(); err != nil {
		return nil, onlinePlayerError(err)
	}
	return result, nil
}

// CreateCharacterIdempotent（创建持久角色）在单事务内完成幂等重放、名称约束、角色写入和Profile拥有关系更新。
func (r *OnlinePlayerRepository) CreateCharacterIdempotent(
	ctx context.Context,
	playerID, creationRequestID, heroDefinitionID, characterName string,
	appearanceSelection map[string]string,
) (playerdata.Character, error) {
	if err := ctx.Err(); err != nil {
		return playerdata.Character{}, err
	}
	name, appearance, err := playerdata.NormalizeCharacterCreation(
		playerID, creationRequestID, heroDefinitionID, characterName, appearanceSelection)
	if err != nil {
		return playerdata.Character{}, err
	}
	if r == nil || !r.PlayerRepository.onlinePlayerAvailable() {
		return playerdata.Character{}, onlinePlayerUnavailable()
	}

	canonical, err := json.Marshal(struct {
		HeroDefinitionID    string            `json:"heroDefinitionId"`
		CharacterName       string            `json:"characterName"`
		AppearanceSelection map[string]string `json:"appearanceSelection"`
	}{heroDefinitionID, name, appearance})
	if err != nil {
		return playerdata.Character{}, onlinePlayerUnavailable()
	}
	appearanceJSON, err := json.Marshal(appearance)
	if err != nil {
		return playerdata.Character{}, onlinePlayerUnavailable()
	}

	tx, err := r.pool.inner.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return playerdata.Character{}, onlinePlayerError(err)
	}
	defer rollbackOnlinePlayer(tx)

	if _, err = tx.Exec(ctx, `SELECT pg_advisory_xact_lock($1)`,
		onlineCharacterLockID(playerID, createCharacterOperation, creationRequestID)); err != nil {
		return playerdata.Character{}, onlinePlayerError(err)
	}

	var previousCanonical []byte
	var previous playerdata.Character
	var previousAppearance []byte
	err = tx.QueryRow(ctx, `SELECT canonical_request,
		character_id,player_id,hero_definition_id,character_name,character_revision,
		onboarding_state,status,appearance_profile_id,appearance_selection
		FROM player_characters WHERE player_id=$1 AND creation_request_id=$2`,
		playerID, creationRequestID).Scan(
		&previousCanonical,
		&previous.CharacterID, &previous.PlayerID, &previous.HeroDefinitionID, &previous.CharacterName,
		&previous.CharacterRevision, &previous.OnboardingState, &previous.Status,
		&previous.AppearanceProfileID, &previousAppearance)
	if err == nil {
		if !bytes.Equal(previousCanonical, canonical) {
			return playerdata.Character{}, apperror.New("CHARACTER_CONFLICT", "创建幂等键已绑定其他角色请求", false)
		}
		if err := json.Unmarshal(previousAppearance, &previous.AppearanceSelection); err != nil {
			return playerdata.Character{}, onlinePlayerUnavailable()
		}
		return previous, nil
	}
	if !errors.Is(err, pgx.ErrNoRows) {
		return playerdata.Character{}, onlinePlayerError(err)
	}

	var ownedJSON []byte
	var profileRevision int64
	err = tx.QueryRow(ctx, `SELECT revision,owned_character_ids FROM player_profiles
		WHERE player_id=$1 FOR UPDATE`, playerID).Scan(&profileRevision, &ownedJSON)
	if errors.Is(err, pgx.ErrNoRows) {
		return playerdata.Character{}, apperror.New("PLAYER_PROFILE_NOT_FOUND", "玩家资料不存在", false)
	}
	if err != nil {
		return playerdata.Character{}, onlinePlayerError(err)
	}

	var duplicateName bool
	if err := tx.QueryRow(ctx, `SELECT EXISTS(
		SELECT 1 FROM player_characters WHERE player_id=$1 AND lower(character_name)=lower($2))`,
		playerID, name).Scan(&duplicateName); err != nil {
		return playerdata.Character{}, onlinePlayerError(err)
	}
	if duplicateName {
		return playerdata.Character{}, apperror.New("CHARACTER_CONFLICT", "角色名称已存在", false)
	}

	character := playerdata.Character{
		CharacterID:         playerdata.CharacterIDForCreation(playerID, creationRequestID),
		PlayerID:            playerID,
		HeroDefinitionID:    heroDefinitionID,
		CharacterName:       name,
		CharacterRevision:   1,
		OnboardingState:     playerdata.OnboardingTutorialRequired,
		Status:              playerdata.CharacterStatusActive,
		AppearanceSelection: appearance,
	}
	_, err = tx.Exec(ctx, `INSERT INTO player_characters
		(character_id,player_id,creation_request_id,canonical_request,hero_definition_id,character_name,
		 character_revision,onboarding_state,status,appearance_profile_id,appearance_selection)
		VALUES($1,$2,$3,$4,$5,$6,1,$7,$8,'',$9::jsonb)`,
		character.CharacterID, playerID, creationRequestID, canonical, heroDefinitionID, name,
		character.OnboardingState, character.Status, string(appearanceJSON))
	if err != nil {
		return playerdata.Character{}, onlinePlayerError(err)
	}

	var owned []string
	if err := json.Unmarshal(ownedJSON, &owned); err != nil {
		return playerdata.Character{}, onlinePlayerUnavailable()
	}
	owned = append(owned, character.CharacterID)
	updatedOwned, err := json.Marshal(owned)
	if err != nil {
		return playerdata.Character{}, onlinePlayerUnavailable()
	}
	command, err := tx.Exec(ctx, `UPDATE player_profiles
		SET owned_character_ids=$2::jsonb,revision=revision+1,updated_at=NOW()
		WHERE player_id=$1 AND revision=$3 AND revision<9223372036854775807`,
		playerID, string(updatedOwned), profileRevision)
	if err != nil {
		return playerdata.Character{}, onlinePlayerError(err)
	}
	if command.RowsAffected() != 1 {
		return playerdata.Character{}, apperror.New("PLAYER_DATA_CONFLICT", "玩家资料Revision冲突", false)
	}
	if err := commitOnlinePlayer(ctx, tx); err != nil {
		return playerdata.Character{}, err
	}
	return character, nil
}

// SelectCharacterIdempotent（权威选择角色）在单事务内提交selected_character_id和可重放结果。
func (r *OnlinePlayerRepository) SelectCharacterIdempotent(
	ctx context.Context,
	playerID, selectionRequestID, characterID string,
	expectedCharacterRevision int64,
) (playerdata.CharacterSelection, error) {
	if err := ctx.Err(); err != nil {
		return playerdata.CharacterSelection{}, err
	}
	if !validOnlinePlayerIdentity(playerID) ||
		!validOnlinePlayerIdentity(selectionRequestID) ||
		!validOnlinePlayerIdentity(characterID) ||
		expectedCharacterRevision < 1 {
		return playerdata.CharacterSelection{}, onlinePlayerInvalid()
	}
	if r == nil || !r.PlayerRepository.onlinePlayerAvailable() {
		return playerdata.CharacterSelection{}, onlinePlayerUnavailable()
	}

	canonical, err := json.Marshal(struct {
		CharacterID               string `json:"characterId"`
		ExpectedCharacterRevision int64  `json:"expectedCharacterRevision"`
	}{characterID, expectedCharacterRevision})
	if err != nil {
		return playerdata.CharacterSelection{}, onlinePlayerUnavailable()
	}

	tx, err := r.pool.inner.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return playerdata.CharacterSelection{}, onlinePlayerError(err)
	}
	defer rollbackOnlinePlayer(tx)

	if _, err = tx.Exec(ctx, `SELECT pg_advisory_xact_lock($1)`,
		onlineCharacterLockID(playerID, selectCharacterOperation, selectionRequestID)); err != nil {
		return playerdata.CharacterSelection{}, onlinePlayerError(err)
	}

	var previousCanonical, snapshot []byte
	err = tx.QueryRow(ctx, `SELECT canonical_request,response_snapshot
		FROM player_character_selection_idempotency
		WHERE player_id=$1 AND selection_request_id=$2`,
		playerID, selectionRequestID).Scan(&previousCanonical, &snapshot)
	if err == nil {
		if !bytes.Equal(previousCanonical, canonical) {
			return playerdata.CharacterSelection{}, apperror.New("CHARACTER_CONFLICT", "选择幂等键已绑定其他角色请求", false)
		}
		var replay playerdata.CharacterSelection
		if json.Unmarshal(snapshot, &replay) != nil ||
			replay.Character.PlayerID != playerID ||
			replay.SelectionRequestID != selectionRequestID {
			return playerdata.CharacterSelection{}, onlinePlayerUnavailable()
		}
		return replay, nil
	}
	if !errors.Is(err, pgx.ErrNoRows) {
		return playerdata.CharacterSelection{}, onlinePlayerError(err)
	}

	character, err := scanOnlineCharacter(tx.QueryRow(ctx, `SELECT
		character_id,player_id,hero_definition_id,character_name,character_revision,
		onboarding_state,status,appearance_profile_id,appearance_selection
		FROM player_characters WHERE player_id=$1 AND character_id=$2 FOR UPDATE`,
		playerID, characterID))
	if errors.Is(err, pgx.ErrNoRows) {
		return playerdata.CharacterSelection{}, apperror.New("CHARACTER_NOT_FOUND", "持久角色不存在或不属于当前玩家", false)
	}
	if err != nil {
		return playerdata.CharacterSelection{}, onlinePlayerError(err)
	}
	if character.Status != playerdata.CharacterStatusActive {
		return playerdata.CharacterSelection{}, apperror.New("CHARACTER_DISABLED", "持久角色当前不可选择", false)
	}
	if character.CharacterRevision != expectedCharacterRevision {
		return playerdata.CharacterSelection{}, apperror.New("CHARACTER_CONFLICT", "角色Revision已变化，请刷新角色列表", false)
	}

	var profileRevision int64
	err = tx.QueryRow(ctx, `UPDATE player_profiles
		SET selected_character_id=$2,revision=revision+1,updated_at=NOW()
		WHERE player_id=$1 AND revision<9223372036854775807
		RETURNING revision`, playerID, characterID).Scan(&profileRevision)
	if errors.Is(err, pgx.ErrNoRows) {
		return playerdata.CharacterSelection{}, apperror.New("PLAYER_PROFILE_NOT_FOUND", "玩家资料不存在", false)
	}
	if err != nil {
		return playerdata.CharacterSelection{}, onlinePlayerError(err)
	}

	result := playerdata.CharacterSelection{
		SelectionRequestID: selectionRequestID,
		ProfileRevision:    profileRevision,
		Character:          character,
	}
	snapshot, err = json.Marshal(result)
	if err != nil {
		return playerdata.CharacterSelection{}, onlinePlayerUnavailable()
	}
	_, err = tx.Exec(ctx, `INSERT INTO player_character_selection_idempotency
		(player_id,selection_request_id,canonical_request,response_snapshot)
		VALUES($1,$2,$3,$4::jsonb)`,
		playerID, selectionRequestID, canonical, string(snapshot))
	if err != nil {
		return playerdata.CharacterSelection{}, onlinePlayerError(err)
	}
	if err := commitOnlinePlayer(ctx, tx); err != nil {
		return playerdata.CharacterSelection{}, err
	}
	return result, nil
}

func scanOnlineCharacter(row pgx.Row) (playerdata.Character, error) {
	var character playerdata.Character
	var appearance []byte
	err := row.Scan(
		&character.CharacterID, &character.PlayerID, &character.HeroDefinitionID, &character.CharacterName,
		&character.CharacterRevision, &character.OnboardingState, &character.Status,
		&character.AppearanceProfileID, &appearance)
	if err != nil {
		return playerdata.Character{}, err
	}
	if len(appearance) == 0 {
		character.AppearanceSelection = map[string]string{}
		return character, nil
	}
	if err := json.Unmarshal(appearance, &character.AppearanceSelection); err != nil {
		return playerdata.Character{}, err
	}
	return character, nil
}

func onlineCharacterLockID(playerID, operation, key string) int64 {
	encoded, _ := json.Marshal([3]string{playerID, operation, key})
	digest := sha256.Sum256(encoded)
	return int64(binary.BigEndian.Uint64(digest[:8]))
}
