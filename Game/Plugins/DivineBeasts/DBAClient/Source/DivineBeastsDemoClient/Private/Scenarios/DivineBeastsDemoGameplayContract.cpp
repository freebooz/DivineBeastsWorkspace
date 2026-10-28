#include "Scenarios/DivineBeastsDemoGameplayScenarios.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "GamePlatformAbilityTags.h"
namespace DivineBeasts::Demo {
FGameplayTag GameplayManualInputTag(){return GamePlatformAbilityTags::InputTestServer.GetTag();}
FDivineBeastsDemoScenarioDefinition GameplayManualDefinition(FName F){ FDivineBeastsDemoScenarioDefinition D;D.FeatureId=F;D.AdapterId=FName(*(TEXT("PlatformGameplay.")+F.ToString()));D.ScenarioId=FName(*(F.ToString()+TEXT(".Manual")));
if(F==TEXT("GamePlatformGameplay.experience")){D.Operations={TEXT("PrepareGameplayHost"),TEXT("BeginExperience"),TEXT("SubscribeExperience"),TEXT("Snapshot"),TEXT("DrainExperience"),TEXT("UnsubscribeExperience"),TEXT("Stop")};
return D;}
if(F==TEXT("GamePlatformGameplay.admission_spawn")){D.Operations={TEXT("PrepareGameplayHost"),TEXT("BeginExperience"),TEXT("SubmitFixtureAdmission"),TEXT("StaleFixtureAdmission"),TEXT("EvaluateSpawn"),TEXT("ConfirmFixturePreparation"),TEXT("StaleFixturePreparation"),TEXT("RequestRestart"),TEXT("RevokeFixtureAdmission"),TEXT("DrainExperience"),TEXT("Snapshot"),TEXT("Stop")};
return D;}
if(F==TEXT("GamePlatformGameplay.eligibility")){D.Operations={TEXT("CreateFixture"),TEXT("SetActive"),TEXT("AdvanceAvatar"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Active");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Boolean;P.DefaultValue.bBooleanValue=true;P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformCharacter.definition")){D.Operations={TEXT("LoadDefinition"),TEXT("ValidateDefinition"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Definition");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAGameplay/Definitions/DA_Hero_Zodiac_Rat.DA_Hero_Zodiac_Rat"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformCharacter.initialize")){D.Operations={TEXT("CreateFixture"),TEXT("InitializeCharacter"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("HeroId");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Identifier;P.DefaultValue.IdentifierValue=TEXT("Hero.Zodiac.Rat");P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("AvatarGeneration");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Integer;P.DefaultValue.IntegerValue=1;P.Minimum=1;P.Maximum=65535;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformCharacter.creation_catalog")){D.Operations={TEXT("ReadCatalog"),TEXT("ValidateDraft"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("HeroId");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Identifier;P.DefaultValue.IdentifierValue=TEXT("Hero.Zodiac.Rat");P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAbilitySystem.avatar_gate")){D.Operations={TEXT("CreateFixture"),TEXT("BindAvatar"),TEXT("SetActive"),TEXT("EvaluateGate"),TEXT("ClearAvatar"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Active");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Boolean;P.DefaultValue.bBooleanValue=true;P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAbilitySystem.tag_input")){D.Operations={TEXT("CreateFixture"),TEXT("BindAvatar"),TEXT("SetActive"),TEXT("GrantInput"),TEXT("Press"),TEXT("Release"),TEXT("ProcessInput"),TEXT("ClearInput"),TEXT("RevokeInput"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Active");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Boolean;P.DefaultValue.bBooleanValue=true;P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("InputTag");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Identifier;P.DefaultValue.IdentifierValue=GameplayManualInputTag().GetTagName();P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAbilitySystem.cast_facts")){D.Operations={TEXT("CreateFixture"),TEXT("PublishCast"),TEXT("DuplicateCast"),TEXT("CancelCast"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("AbilityId");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Identifier;P.DefaultValue.IdentifierValue=TEXT("Demo.Manual.Cast");P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Duration");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=2;P.Minimum=0;P.Maximum=10;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAbilitySystem.abilityset_definition")){D.Operations={TEXT("LoadDefinition"),TEXT("ValidateDefinition"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Definition");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAGameplay/Abilities/Placeholders/DA_DBA_Rat_PlaceholderAbilitySet.DA_DBA_Rat_PlaceholderAbilitySet"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformCombat.damage_heal")){D.Operations={TEXT("CreateFixture"),TEXT("ActivateFixture"),TEXT("Damage"),TEXT("DuplicateDamage"),TEXT("Heal"),TEXT("StaleDamage"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Magnitude");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=25;P.Minimum=0;P.Maximum=100000;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformCombat.shield")){D.Operations={TEXT("CreateFixture"),TEXT("ActivateFixture"),TEXT("Shield"),TEXT("Damage"),TEXT("DuplicateDamage"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Magnitude");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=25;P.Minimum=0;P.Maximum=100000;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Duration");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=3;P.Minimum=0.01;P.Maximum=10;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformCombat.control_displacement")){D.Operations={TEXT("CreateFixture"),TEXT("ActivateFixture"),TEXT("Stun"),TEXT("Silence"),TEXT("Slow"),TEXT("RemoveControl"),TEXT("Push"),TEXT("Pull"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Duration");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=2;P.Minimum=1;P.Maximum=10;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("SlowFraction");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=0.3;P.Minimum=0.05;P.Maximum=0.7;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Speed");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=300;P.Minimum=1;P.Maximum=2000;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformCombat.projectile")){D.Operations={TEXT("CreateFixture"),TEXT("ActivateFixture"),TEXT("LaunchMovingTarget"),TEXT("LaunchThenSwitch"),TEXT("LaunchThenInvalidate"),TEXT("LaunchStraight"),TEXT("LaunchBlocked"),TEXT("SelectTarget"),TEXT("ClearSelection"),TEXT("Launch"),TEXT("MoveTarget"),TEXT("SwitchSelection"),TEXT("InvalidateTarget"),TEXT("ToggleObstacle"),TEXT("CancelProjectile"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Magnitude");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=25;P.Minimum=0;P.Maximum=100000;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("TargetX");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=350;P.Minimum=-1500;P.Maximum=1500;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("TargetY");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=100;P.Minimum=-1000;P.Maximum=1000;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Obstacle");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Boolean;P.DefaultValue.bBooleanValue=false;P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformCombat.energy")){D.Operations={TEXT("CreateFixture"),TEXT("ActivateFixture"),TEXT("CommitInput"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
return D;}
if(F==TEXT("GamePlatformAnimation.locomotion")){D.Operations={TEXT("CreateFixture"),TEXT("LoadAnimation"),TEXT("MoveCharacter"),TEXT("JumpCharacter"),TEXT("StopMovement"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Mesh");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("AnimClass");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_WorldLocomotion.ABP_DBA_WorldLocomotion_C"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Speed");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=200;P.Minimum=-600;P.Maximum=600;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAnimation.hitstop")){D.Operations={TEXT("CreateFixture"),TEXT("LoadAnimation"),TEXT("Hitstop"),TEXT("DuplicateHitstop"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Mesh");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("AnimClass");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_WorldLocomotion.ABP_DBA_WorldLocomotion_C"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Frames");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Integer;P.DefaultValue.IntegerValue=4;P.Minimum=0;P.Maximum=10;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAnimation.hitflash")){D.Operations={TEXT("CreateFixture"),TEXT("LoadOverlay"),TEXT("HitFlash"),TEXT("DuplicateHitFlash"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Overlay");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/Gameplay/M_DBA_ManualHitFlash.M_DBA_ManualHitFlash"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Duration");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.DefaultValue.NumberValue=0.033;P.Minimum=0;P.Maximum=0.0333333;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAI.brain")){D.Operations={TEXT("CreateFixture"),TEXT("LoadAIDefinition"),TEXT("PossessAI"),TEXT("ForceDecision"),TEXT("UnPossessAI"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Definition");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/Gameplay/DA_DBA_ManualAI.DA_DBA_ManualAI"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAI.targets")){D.Operations={TEXT("CreateFixture"),TEXT("SetTargetEnabled"),TEXT("AdvanceTarget"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Enabled");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Boolean;P.DefaultValue.bBooleanValue=true;P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformAI.attack")){D.Operations={TEXT("CreateFixture"),TEXT("LoadAIDefinition"),TEXT("PossessAI"),TEXT("ForceDecision"),TEXT("TryAttack"),TEXT("UnPossessAI"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Definition");P.DefaultValue.Type=EDivineBeastsDemoParameterType::AssetPath;P.DefaultValue.AssetPathValue=FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/Gameplay/DA_DBA_ManualAI.DA_DBA_ManualAI"));P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
if(F==TEXT("GamePlatformQuest.quest_progress")){D.Operations={TEXT("InspectQuestHost"),TEXT("Snapshot"),TEXT("Stop")};
return D;}
if(F==TEXT("GamePlatformQuest.persistence")){D.Operations={TEXT("InspectPersistenceHost"),TEXT("Snapshot"),TEXT("Stop")};
return D;}
if(F==TEXT("GamePlatformQuest.tracking")){D.Operations={TEXT("ReadQuests"),TEXT("TrackQuest"),TEXT("UntrackQuest"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
{FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("QuestId");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Identifier;P.DefaultValue.IdentifierValue=TEXT("Demo.Manual.Quest");P.Minimum=0;P.Maximum=0;D.Parameters.Add(P);}
return D;}
return {}; }
TArray<FName> GameplayManualFeatures(){return {TEXT("GamePlatformGameplay.experience"),TEXT("GamePlatformGameplay.admission_spawn"),TEXT("GamePlatformGameplay.eligibility"),TEXT("GamePlatformCharacter.definition"),TEXT("GamePlatformCharacter.initialize"),TEXT("GamePlatformCharacter.creation_catalog"),TEXT("GamePlatformAbilitySystem.avatar_gate"),TEXT("GamePlatformAbilitySystem.tag_input"),TEXT("GamePlatformAbilitySystem.cast_facts"),TEXT("GamePlatformAbilitySystem.abilityset_definition"),TEXT("GamePlatformCombat.damage_heal"),TEXT("GamePlatformCombat.shield"),TEXT("GamePlatformCombat.control_displacement"),TEXT("GamePlatformCombat.projectile"),TEXT("GamePlatformCombat.energy"),TEXT("GamePlatformAnimation.locomotion"),TEXT("GamePlatformAnimation.hitstop"),TEXT("GamePlatformAnimation.hitflash"),TEXT("GamePlatformAI.brain"),TEXT("GamePlatformAI.targets"),TEXT("GamePlatformAI.attack"),TEXT("GamePlatformQuest.quest_progress"),TEXT("GamePlatformQuest.persistence"),TEXT("GamePlatformQuest.tracking")};}
bool ValidateGameplayManualContract(const FDivineBeastsDemoScenarioDefinition& D){auto E=GameplayManualDefinition(D.FeatureId);if(E.FeatureId.IsNone()||E.AdapterId!=D.AdapterId||E.ScenarioId!=D.ScenarioId||E.Operations.Num()!=D.Operations.Num()||E.Parameters.Num()!=D.Parameters.Num())return false;TSet<FName> Seen;for(auto O:D.Operations){if(!E.Operations.Contains(O)||Seen.Contains(O))return false;Seen.Add(O);}for(int32 I=0;I<E.Parameters.Num();++I)if(E.Parameters[I].ParameterId!=D.Parameters[I].ParameterId||E.Parameters[I].DefaultValue.Type!=D.Parameters[I].DefaultValue.Type||E.Parameters[I].Minimum!=D.Parameters[I].Minimum||E.Parameters[I].Maximum!=D.Parameters[I].Maximum)return false;return true;}
FGamePlatformResult ValidateGameplayManualValues(const FDivineBeastsDemoScenarioDefinition& D,const TArray<FDivineBeastsDemoParameterValue>& V){if(!ValidateGameplayManualContract(D)||D.Parameters.Num()!=V.Num())return FGamePlatformResult::Failure(TEXT("Demo.Gameplay.Contract"),TEXT("固定参数合同不匹配。"));for(int32 I=0;I<V.Num();++I){auto& X=V[I];auto& P=D.Parameters[I];bool B=X.Type==P.DefaultValue.Type;if(X.Type==EDivineBeastsDemoParameterType::Number)B&=FMath::IsFinite(X.NumberValue)&&X.NumberValue>=P.Minimum&&X.NumberValue<=P.Maximum;if(X.Type==EDivineBeastsDemoParameterType::Integer)B&=X.IntegerValue>=P.Minimum&&X.IntegerValue<=P.Maximum;if(X.Type==EDivineBeastsDemoParameterType::AssetPath)B&=X.AssetPathValue.IsValid()&&X.AssetPathValue.GetSubPathString().IsEmpty();if(X.Type==EDivineBeastsDemoParameterType::Identifier){FString T=X.IdentifierValue.ToString();B&=!X.IdentifierValue.IsNone()&&T.Len()<=128;for(auto C:T)B&=(C>='A'&&C<='Z')||(C>='a'&&C<='z')||(C>='0'&&C<='9')||C=='.'||C=='_'||C=='-';}if(!B)return FGamePlatformResult::Failure(TEXT("Demo.Gameplay.Parameter"),TEXT("类型、有限范围、封闭标识或资源路径不合法。"));}return FGamePlatformResult::Success();}
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterGameplayManualScenarios(UDivineBeastsDemoCatalogSubsystem& C){TArray<FDivineBeastsDemoAdapterRegistrationHandle> Owned;for(auto F:GameplayManualFeatures()){auto H=C.RegisterScenarioAdapter(FName(*(TEXT("PlatformGameplay.")+F.ToString())),[F](UWorld& W){return CreateGameplayManualScenario(W,F);});if(!H.IsValid()){for(auto& Prior:Owned)C.UnregisterScenarioAdapter(Prior);return {};}Owned.Add(H);}return Owned;}
}
