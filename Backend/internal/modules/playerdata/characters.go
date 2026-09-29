package playerdata

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"strings"
	"unicode/utf8"

	"divinebeasts/backend/internal/platform/apperror"
)

const (
	// CharacterStatusActive（可用角色状态）表示角色允许被玩家选择进入游戏。
	CharacterStatusActive = "Active"
	// CharacterStatusDisabled（禁用角色状态）表示角色存在但暂不允许进入游戏。
	CharacterStatusDisabled = "Disabled"

	// OnboardingTutorialRequired（需要新手教学）是新创建角色的默认引导状态。
	OnboardingTutorialRequired = "TutorialRequired"
)

// Character（持久角色）属于玩家长期数据，不承载生命值、技能冷却等实时战斗状态。
type Character struct {
	CharacterID         string            `json:"characterId"`
	PlayerID            string            `json:"-"`
	HeroDefinitionID    string            `json:"heroDefinitionId"`
	CharacterName       string            `json:"characterName"`
	CharacterRevision   int64             `json:"characterRevision"`
	OnboardingState     string            `json:"onboardingState"`
	Status              string            `json:"status"`
	AppearanceProfileID string            `json:"appearanceProfileId,omitempty"`
	AppearanceSelection map[string]string `json:"-"`
}

// CharacterSelection（权威角色选择结果）同时返回新的资料Revision，供后续世界准入使用。
type CharacterSelection struct {
	SelectionRequestID string    `json:"selectionRequestId"`
	ProfileRevision    int64     `json:"profileRevision"`
	Character          Character `json:"character"`
}

// CharacterRepository（角色仓储端口）由玩家资料服务拥有；Gateway和UE客户端不得越过该端口直接访问数据库。
type CharacterRepository interface {
	ListCharacters(ctx context.Context, playerID string) ([]Character, error)
	CreateCharacterIdempotent(
		ctx context.Context,
		playerID, creationRequestID, heroDefinitionID, characterName string,
		appearanceSelection map[string]string,
	) (Character, error)
	SelectCharacterIdempotent(
		ctx context.Context,
		playerID, selectionRequestID, characterID string,
		expectedCharacterRevision int64,
	) (CharacterSelection, error)
}

// ListCharacters（获取角色列表）只返回当前认证玩家自己的角色，并保持仓储确定顺序。
func (s *Service) ListCharacters(ctx context.Context, playerID string) ([]Character, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if !validIdentity(playerID) {
		return nil, invalidRequest()
	}
	repo, ok := s.repo.(CharacterRepository)
	if !ok {
		return nil, unavailable()
	}
	items, err := repo.ListCharacters(ctx, playerID)
	if err != nil {
		return nil, classifyOnlineError(err)
	}
	return cloneCharacters(items), nil
}

// CreateCharacter（创建持久角色）使用creationRequestID实现玩家作用域幂等。
// 同键同规范内容必须重放原角色；同键异内容必须返回冲突，不能创建第二个角色。
func (s *Service) CreateCharacter(
	ctx context.Context,
	playerID, creationRequestID, heroDefinitionID, characterName string,
	appearanceSelection map[string]string,
) (Character, error) {
	if err := ctx.Err(); err != nil {
		return Character{}, err
	}
	name, appearance, err := NormalizeCharacterCreation(
		playerID,
		creationRequestID,
		heroDefinitionID,
		characterName,
		appearanceSelection,
	)
	if err != nil {
		return Character{}, err
	}
	repo, ok := s.repo.(CharacterRepository)
	if !ok {
		return Character{}, unavailable()
	}
	character, err := repo.CreateCharacterIdempotent(
		ctx, playerID, creationRequestID, heroDefinitionID, name, appearance)
	if err != nil {
		return Character{}, classifyOnlineError(err)
	}
	return cloneCharacter(character), nil
}

// SelectCharacter（选择持久角色）只有仓储完成归属、状态和Revision验证后才返回成功。
// 客户端点击卡片不是权威选择；成功结果会同时推进玩家资料Revision。
func (s *Service) SelectCharacter(
	ctx context.Context,
	playerID, selectionRequestID, characterID string,
	expectedCharacterRevision int64,
) (CharacterSelection, error) {
	if err := ctx.Err(); err != nil {
		return CharacterSelection{}, err
	}
	if !validIdentity(playerID) ||
		!validBoundedID(selectionRequestID, 128) ||
		!validBoundedID(characterID, 256) ||
		expectedCharacterRevision < 1 {
		return CharacterSelection{}, invalidRequest()
	}
	repo, ok := s.repo.(CharacterRepository)
	if !ok {
		return CharacterSelection{}, unavailable()
	}
	result, err := repo.SelectCharacterIdempotent(
		ctx, playerID, selectionRequestID, characterID, expectedCharacterRevision)
	if err != nil {
		return CharacterSelection{}, classifyOnlineError(err)
	}
	result.Character = cloneCharacter(result.Character)
	return result, nil
}

// NormalizeCharacterCreation（规范化角色创建请求）集中执行领域输入约束，仓储可再次调用防止绕过Service。
// 外观选项采用有限白名单形态：最多32项，每个键和值最多64个Unicode字符。
func NormalizeCharacterCreation(
	playerID, creationRequestID, heroDefinitionID, characterName string,
	appearanceSelection map[string]string,
) (string, map[string]string, error) {
	name := strings.TrimSpace(characterName)
	if !validIdentity(playerID) ||
		!validBoundedID(creationRequestID, 128) ||
		!validBoundedID(heroDefinitionID, 256) ||
		!utf8.ValidString(name) ||
		strings.ContainsRune(name, 0) ||
		name == "" ||
		utf8.RuneCountInString(name) > 24 ||
		len(appearanceSelection) > 32 {
		return "", nil, invalidRequest()
	}

	normalized := make(map[string]string, len(appearanceSelection))
	for key, value := range appearanceSelection {
		key = strings.TrimSpace(key)
		value = strings.TrimSpace(value)
		if !validBoundedID(key, 64) ||
			!utf8.ValidString(value) ||
			strings.ContainsRune(value, 0) ||
			utf8.RuneCountInString(value) > 64 {
			return "", nil, invalidRequest()
		}
		normalized[key] = value
	}
	return name, normalized, nil
}

// CharacterIDForCreation（生成角色ID）由玩家身份和创建幂等键确定。
// 相同玩家相同请求即使在提交结果未知后重试，也会指向同一角色ID。
func CharacterIDForCreation(playerID, creationRequestID string) string {
	sum := sha256.Sum256([]byte(playerID + "\x00" + creationRequestID))
	return "character-" + hex.EncodeToString(sum[:16])
}

// canonicalAppearance（规范外观快照）用于内存幂等比较；encoding/json会稳定排序字符串Map键。
func canonicalAppearance(value map[string]string) string {
	encoded, _ := json.Marshal(value)
	return string(encoded)
}

func validBoundedID(value string, maxRunes int) bool {
	return validIdentity(value) && utf8.RuneCountInString(value) <= maxRunes
}

func characterNotFound() error {
	return apperror.New("CHARACTER_NOT_FOUND", "持久角色不存在或不属于当前玩家", false)
}
func characterConflict(message string) error {
	return apperror.New("CHARACTER_CONFLICT", message, false)
}
func characterDisabled() error {
	return apperror.New("CHARACTER_DISABLED", "持久角色当前不可选择", false)
}

type memoryCreateRecord struct {
	HeroDefinitionID string
	CharacterName    string
	AppearanceJSON   string
	CharacterID      string
}

type memorySelectionRecord struct {
	CharacterID               string
	ExpectedCharacterRevision int64
	Result                    CharacterSelection
}

// ensureCharacterStorageLocked（初始化角色内存结构）必须在MemoryRepository锁内调用。
// 兼容早期通过结构字面量创建MemoryRepository的测试，避免nil map写入。
func (r *MemoryRepository) ensureCharacterStorageLocked() {
	if r.characters == nil {
		r.characters = map[string]Character{}
	}
	if r.createRecords == nil {
		r.createRecords = map[string]memoryCreateRecord{}
	}
	if r.selectionRecords == nil {
		r.selectionRecords = map[string]memorySelectionRecord{}
	}
}

// ListCharacters（内存实现）按Profile.OwnedCharacterIDs顺序返回，保证UI列表稳定且不暴露其他玩家角色。
func (r *MemoryRepository) ListCharacters(_ context.Context, playerID string) ([]Character, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	profile, ok := r.items[playerID]
	if !ok {
		return nil, characterNotFound()
	}
	result := make([]Character, 0, len(profile.OwnedCharacterIDs))
	for _, id := range profile.OwnedCharacterIDs {
		if character, exists := r.characters[id]; exists && character.PlayerID == playerID {
			result = append(result, cloneCharacter(character))
		}
	}
	return result, nil
}

// CreateCharacterIdempotent（内存实现）在同一写锁内完成幂等重放、重名检查、角色写入和Profile更新。
func (r *MemoryRepository) CreateCharacterIdempotent(
	_ context.Context,
	playerID, creationRequestID, heroDefinitionID, characterName string,
	appearanceSelection map[string]string,
) (Character, error) {
	name, normalized, err := NormalizeCharacterCreation(
		playerID, creationRequestID, heroDefinitionID, characterName, appearanceSelection)
	if err != nil {
		return Character{}, err
	}

	r.mu.Lock()
	defer r.mu.Unlock()
	r.ensureCharacterStorageLocked()
	profile, ok := r.items[playerID]
	if !ok {
		return Character{}, characterNotFound()
	}

	recordKey := playerID + "\x00" + creationRequestID
	appearanceJSON := canonicalAppearance(normalized)
	if previous, exists := r.createRecords[recordKey]; exists {
		if previous.HeroDefinitionID != heroDefinitionID ||
			previous.CharacterName != name ||
			previous.AppearanceJSON != appearanceJSON {
			return Character{}, characterConflict("创建幂等键已绑定其他角色请求")
		}
		character, exists := r.characters[previous.CharacterID]
		if !exists {
			return Character{}, unavailable()
		}
		return cloneCharacter(character), nil
	}

	for _, id := range profile.OwnedCharacterIDs {
		if existing, exists := r.characters[id]; exists &&
			strings.EqualFold(existing.CharacterName, name) {
			return Character{}, characterConflict("角色名称已存在")
		}
	}

	character := Character{
		CharacterID:         CharacterIDForCreation(playerID, creationRequestID),
		PlayerID:            playerID,
		HeroDefinitionID:    heroDefinitionID,
		CharacterName:       name,
		CharacterRevision:   1,
		OnboardingState:     OnboardingTutorialRequired,
		Status:              CharacterStatusActive,
		AppearanceSelection: cloneStringMap(normalized),
	}
	r.characters[character.CharacterID] = character
	r.createRecords[recordKey] = memoryCreateRecord{
		HeroDefinitionID: heroDefinitionID,
		CharacterName:    name,
		AppearanceJSON:   appearanceJSON,
		CharacterID:      character.CharacterID,
	}
	profile.OwnedCharacterIDs = append(profile.OwnedCharacterIDs, character.CharacterID)
	profile.Revision++
	r.items[playerID] = cloneProfile(profile)
	return cloneCharacter(character), nil
}

// SelectCharacterIdempotent（内存实现）在同一写锁内验证并更新Profile.SelectedCharacterID。
func (r *MemoryRepository) SelectCharacterIdempotent(
	_ context.Context,
	playerID, selectionRequestID, characterID string,
	expectedCharacterRevision int64,
) (CharacterSelection, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.ensureCharacterStorageLocked()

	recordKey := playerID + "\x00" + selectionRequestID
	if previous, exists := r.selectionRecords[recordKey]; exists {
		if previous.CharacterID != characterID ||
			previous.ExpectedCharacterRevision != expectedCharacterRevision {
			return CharacterSelection{}, characterConflict("选择幂等键已绑定其他角色请求")
		}
		result := previous.Result
		result.Character = cloneCharacter(result.Character)
		return result, nil
	}

	profile, ok := r.items[playerID]
	if !ok {
		return CharacterSelection{}, characterNotFound()
	}
	character, ok := r.characters[characterID]
	if !ok || character.PlayerID != playerID {
		return CharacterSelection{}, characterNotFound()
	}
	if character.Status != CharacterStatusActive {
		return CharacterSelection{}, characterDisabled()
	}
	if character.CharacterRevision != expectedCharacterRevision {
		return CharacterSelection{}, characterConflict("角色Revision已变化，请刷新角色列表")
	}

	profile.SelectedCharacterID = character.CharacterID
	profile.Revision++
	r.items[playerID] = cloneProfile(profile)
	result := CharacterSelection{
		SelectionRequestID: selectionRequestID,
		ProfileRevision:    profile.Revision,
		Character:          cloneCharacter(character),
	}
	r.selectionRecords[recordKey] = memorySelectionRecord{
		CharacterID:               characterID,
		ExpectedCharacterRevision: expectedCharacterRevision,
		Result:                    result,
	}
	return result, nil
}

func cloneCharacter(value Character) Character {
	value.AppearanceSelection = cloneStringMap(value.AppearanceSelection)
	return value
}
func cloneCharacters(values []Character) []Character {
	result := make([]Character, len(values))
	for i := range values {
		result[i] = cloneCharacter(values[i])
	}
	return result
}
func cloneStringMap(value map[string]string) map[string]string {
	if value == nil {
		return map[string]string{}
	}
	result := make(map[string]string, len(value))
	for key, item := range value {
		result[key] = item
	}
	return result
}
