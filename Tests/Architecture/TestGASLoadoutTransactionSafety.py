#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM GAS 技能集授权事务及跨角色回调的源码反向门禁。

只验证已写入的请求身份、资源租约和旧授权账本摘除顺序；
真正的GAS同步委托重入与多人网络行为必须再执行UE5.8原生自动化。
所有故障注入都发生在内存字符串，不修改真实源码/蓝图。
"""
from __future__ import annotations
import unittest
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / ("Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                 "DivineBeastsAbilitiesRuntime/Private/Components/"
                 "DivineBeastsAbilityLoadoutComponent.cpp")


def body(source: str, start: str, next_method: str) -> str:
    """提取单个私有方法，避免其他方法的同名保护语句制造误通过。"""
    i = source.find(start)
    j = source.find(next_method, i + len(start))
    if i < 0 or j <= i:
        return ""
    return source[i:j]



def validate_grant_scope(source: str) -> bool:
    """验证服务端技能预检的C++基本括号配对和两项关键作用域，防止文本插入破坏构建。

    不把该结果冒充C++编译；该检查只针对当前ApplyAbilitySet实际实现。
    """
    source = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"', " ", source)
    stack = []
    match = {}
    for index, char in enumerate(source):
        if char == "{":
            stack.append(index)
        elif char == "}":
            if not stack:
                return False
            match[stack.pop()] = index
    if stack:
        return False

    start = source.find("if (Balance.CooldownSeconds > 0.0f || CooldownEffect)")
    brace = source.find("{", start)
    period = source.find("const float CooldownPeriod =")
    input_tag = source.find("if (Grant.InputTag.IsValid())", period)
    if start < 0 or brace < 0 or period < 0 or input_tag < 0:
        return False
    if brace not in match or not (brace < period < match[brace] < input_tag):
        return False
    # 未识别的Ability不得进入使用Configured的后续分支。
    return re.search(r"if\s*\(!Configured\)\s*\{[^{}]*\}\s*else\s*\{", source) is not None


def checks(source: str) -> dict[str, bool]:
    refresh = body(source,
                   "bool UDivineBeastsAbilityLoadoutComponent::AuthorityRefreshFromCharacter(",
                   "void UDivineBeastsAbilityLoadoutComponent::HandleAbilitySetLoaded(")
    ready = body(source,
                 "void UDivineBeastsAbilityLoadoutComponent::HandleAbilitySetLoaded(",
                 "bool UDivineBeastsAbilityLoadoutComponent::IsStartupEffectReversible(")
    grant = body(source,
                 "bool UDivineBeastsAbilityLoadoutComponent::ApplyAbilitySet(",
                 "void UDivineBeastsAbilityLoadoutComponent::RevokeOwnedGrants(")
    revoke = body(source,
                  "void UDivineBeastsAbilityLoadoutComponent::RevokeOwnedGrants(",
                  "void UDivineBeastsAbilityLoadoutComponent::PublishLoadout(")
    not_ready = body(source,
                     "void UDivineBeastsAbilityLoadoutComponent::HandleCharacterReadinessChanged(",
                     "bool UDivineBeastsAbilityLoadoutComponent::AuthorityRefreshFromCharacter(")
    begin = body(source, "void UDivineBeastsAbilityLoadoutComponent::BeginPlay(",
                 "void UDivineBeastsAbilityLoadoutComponent::EndPlay(")
    ending = body(source, "void UDivineBeastsAbilityLoadoutComponent::EndPlay(",
                  "void UDivineBeastsAbilityLoadoutComponent::GetLifetimeReplicatedProps(")
    replicated = body(source, "void UDivineBeastsAbilityLoadoutComponent::OnRep_LoadoutState(",
                      "IGamePlatformDataService* UDivineBeastsAbilityLoadoutComponent::FindDataService(")
    published = source[source.find(
        "void UDivineBeastsAbilityLoadoutComponent::PublishLoadout("):]
    acquisition = refresh[refresh.find("const FGamePlatformDataLease NewLease = Data->AcquireDefinition("):]

    def strictly_ordered(haystack: str, *needles: str) -> bool:
        position = -1
        for needle in needles:
            position = haystack.find(needle, position + 1)
            if position < 0:
                return False
        return True

    return {
        "endplay_closes_before_external_notifications": all(x in (
            begin + ending + replicated + not_ready + refresh + ready + grant + published
        ) for x in (
            "bEndingPlay = false", "bEndingPlay = true",
            "if (bEndingPlay) return", "if (bEndingPlay || !IsValid(Owner)",
            "!bEndingPlay && IsValid(CurrentOwner)",
        )) and strictly_ordered(ending, "bEndingPlay = true",
            "++RequestSerial", "Changed.Clear()", "RevokeOwnedGrants()")
            and "if (bEndingPlay || !IsValid(GetOwner())" in not_ready,
        "acquire_lease_cannot_overwrite_successor": strictly_ordered(
            acquisition,
            "const FGamePlatformDataLease NewLease = Data->AcquireDefinition(",
            "if (!IsRequestCurrent() || PendingSetId != RequestedSetId)",
            "Data->ReleaseDefinition(NewLease)",
            "DefinitionLease = NewLease")
            and "if (!AcquireResult.IsSuccess() || !NewLease.IsValid())" in acquisition,
        "closing_cannot_publish_new_ready": strictly_ordered(
            published, "if (bEndingPlay) return",
            "const int64 PublishedRevision = LoadoutState.Revision",
            "Changed.Broadcast(LoadoutState)",
            "if (bEndingPlay || LoadoutState.Revision != PublishedRevision) return"),
        "stale_not_ready_cannot_override_successor": strictly_ordered(
            not_ready,
            "const int32 ExpectedSerial = ++RequestSerial",
            "RevokeOwnedGrants();",
            "if (ExpectedSerial != RequestSerial",
            "PublishLoadout(NAME_None, 0, false, {})"),
        "old_request_cannot_publish_new_avatar": all(x in refresh for x in (
            "const int32 Serial = ++RequestSerial",
            "if (!IsRequestCurrent())",
            "PublishLoadout(ExpectedHero, ExpectedAvatar, false, {})",
            "PendingSetId = RequestedSetId",
        )) and strictly_ordered(refresh,
            "const int32 Serial = ++RequestSerial", "RevokeOwnedGrants();",
            "if (!IsRequestCurrent())", "PublishLoadout(ExpectedHero, ExpectedAvatar, false, {})",
            "if (!IsRequestCurrent())", "PendingSetId = RequestedSetId"),
        "full_signed_lease": all(
            f"CompletedLease.{field} != DefinitionLease.{field}" in ready
            for field in ("ScopeId", "LeaseId", "Generation", "IssuerProof",
                          "DefinitionId", "ResourcePaths", "Bundles")),
        "stale_failure_cannot_revoke_successor": all(x in ready for x in (
            "ExpectedRequestSerial == RequestSerial",
            "if (!IsRequestCurrent()) return;",
            "if (IsRequestCurrent()) PublishLoadout(ExpectedHero, ExpectedAvatar, false, {});",
            "if (!IsRequestCurrent() || !DefinitionLease.IsValid()) return;",
        )) and strictly_ordered(ready,
            "if (!Set || !ApplyAbilitySet(*Set, Error))",
            "if (!IsRequestCurrent()) return;",
            "RevokeOwnedGrants();",
            "if (IsRequestCurrent()) PublishLoadout(ExpectedHero, ExpectedAvatar, false, {});"),

        "grant_cost_cooldown_scope_well_formed": validate_grant_scope(grant),
        "granted_set_matches_signed_asset_id": (
            "const FPrimaryAssetId ExpectedSetAssetId(" in grant
            and "Set.GetPrimaryAssetId() != ExpectedSetAssetId" in grant
            and "UGamePlatformPrimaryDataAsset::DefinitionAssetType(), GrantSetId" in grant),
        "resolved_ability_class_has_no_duplicate_or_client_only_execution": all(
            marker in grant for marker in (
                "TSet<const UClass*> NewAbilityClasses",
                "NewAbilityClasses.Contains(AbilityClass)",
                "NewAbilityClasses.Add(AbilityClass)",
                "AbilityClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)",
                "GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced",
                "GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly",
            )),
        "cost_effect_rejects_hidden_side_effects": all(marker in grant for marker in (
            "CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant",
            "!FMath::IsNearlyZero(CostPeriod)",
            "!CostEffect->Executions.IsEmpty()",
            "CostEffect->FindComponent(UGameplayEffectComponent::StaticClass()) != nullptr",
            "ActualMomentumChange, -Balance.MomentumCost",
        )) and re.search(
            r"Modifier\.Attribute\s*!=\s*UDivineBeastsMomentumAttributeSet::GetMomentumAttribute\(\)",
            grant) is not None,
        "cooldown_effect_rejects_hidden_modifier_and_executions": all(
            marker in grant for marker in (
                "CooldownEffect->DurationPolicy != EGameplayEffectDurationType::HasDuration",
                "!CooldownEffect->DurationMagnitude.GetStaticMagnitudeIfPossible(",
                "!CooldownEffect->Modifiers.IsEmpty()",
                "!CooldownEffect->Executions.IsEmpty()",
                "!FMath::IsNearlyZero(CooldownPeriod)",
            )),
        "old_grant_effect_cleanup": strictly_ordered(
            grant, "const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(",
            "if (!Handle.IsValid())", "if (!IsGrantCurrent())",
            "ASC->RemoveActiveGameplayEffect(Handle)", "OwnedEffectHandles.Add(Handle)"),
        "old_grant_spec_cleanup": strictly_ordered(
            grant, "const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);",
            "if (!Handle.IsValid())", "if (!IsGrantCurrent())",
            "ASC->CancelAbilityHandle(Handle)", "ASC->ClearAbility(Handle)",
            "OwnedAbilityHandles.Add(Handle)"),
        "lease_detached_before_external_callbacks": strictly_ordered(
            revoke, "MoveTemp(OwnedAbilityHandles)", "MoveTemp(OwnedEffectHandles)",
            "const FGamePlatformDataLease LeaseToRelease = DefinitionLease",
            "OwnedAbilityHandles.Reset()", "OwnedEffectHandles.Reset()",
            "DefinitionLease = {}", "PendingSetId = NAME_None",
            "ASC->CancelAbilityHandle(Handle)",
            "ASC->RemoveActiveGameplayEffect(Handle)",
            "Data->ReleaseDefinition(LeaseToRelease)"),
    }


class LoadoutTransactionSourceGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = SOURCE.read_text(encoding="utf-8-sig")

    def test_current_transaction_contract(self):
        current = checks(self.original)
        self.assertTrue(all(current.values()), current)

    def test_ending_flag_not_removed(self):
        changed = self.original.replace("bEndingPlay = true;", "// closing flag removed", 1)
        self.assertFalse(checks(changed)["endplay_closes_before_external_notifications"])

    def test_old_acquire_does_not_overwrite_newer_lease(self):
        changed = self.original.replace(
            "const FGamePlatformDataLease NewLease = Data->AcquireDefinition(",
            "DefinitionLease = Data->AcquireDefinition(", 1)
        self.assertFalse(checks(changed)["acquire_lease_cannot_overwrite_successor"])

    def test_after_endplay_no_stale_publish(self):
        changed = self.original.replace(
            "if (bEndingPlay) return; // 退出期绝不由旧委托发布新授权或空状态。",
            "// removed closing check", 1)
        self.assertFalse(checks(changed)["closing_cannot_publish_new_ready"])

    def test_old_not_ready_must_not_clear_successor(self):
        original = self.original
        self.assertFalse(checks(original.replace(
            "if (ExpectedSerial != RequestSerial || !IsValid(GetOwner()) ||",
            "if (false && !IsValid(GetOwner()) ||", 1))[
                "stale_not_ready_cannot_override_successor"])

    def test_native_reentry_uses_actual_gas_effect_callback(self):
        runtime = ROOT / ("Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                          "DivineBeastsAbilitiesRuntime/Private/Tests/"
                          "DivineBeastsAbilityLoadoutReentryTests.cpp")
        header = SOURCE.parents[2] / "Public/Components/DivineBeastsAbilityLoadoutComponent.h"
        self.assertTrue(runtime.is_file())
        self.assertTrue(header.is_file())
        native = runtime.read_text(encoding="utf-8-sig")
        interface = header.read_text(encoding="utf-8-sig")
        for contract in (
            "OnAnyGameplayEffectRemovedDelegate().AddLambda(",
            "ApplyGameplayEffectToSelf(",
            "GetActiveGameplayEffect(Old)",
            "GetActiveGameplayEffect(Successor)",
            "HandleCharacterReadinessChanged(false)",
            "NativeRemovedEffectPreservesSuccessor",
            "NativeReadinessCallbackCannotClearSuccessor",
            "NativeEndPlayCallbackRejectsNewGrant",
            "bClosingWasVisible = Loadout->bEndingPlay",
        ):
            self.assertIn(contract, native)
        self.assertIn("friend class FDivineBeastsGASOwnedGrantReentryTest", interface)
        self.assertIn("friend class FDivineBeastsGASReadinessReentryTest", interface)
        self.assertIn("friend class FDivineBeastsGASEndPlayReentryTest", interface)



    def test_cooldown_policy_outside_scope_rejected(self):
        changed = self.original.replace(
            "const float CooldownPeriod = CooldownEffect->Period.GetValueAtLevel(Grant.AbilityLevel);",
            "const float WrongScopePeriod = CooldownEffect->Period.GetValueAtLevel(Grant.AbilityLevel);", 1)
        self.assertFalse(checks(changed)["grant_cost_cooldown_scope_well_formed"])

    def test_dangling_configured_else_rejected(self):
        changed = self.original.replace(
            "else\n        {\n            // 项目能力在实例内记录每次Commit资格",
            "if (true)\n        {\n            // 项目能力在实例内记录每次Commit资格", 1)
        self.assertNotEqual(changed, self.original)
        self.assertFalse(checks(changed)["grant_cost_cooldown_scope_well_formed"])

    def test_wrong_granted_set_asset_identity_rejected(self):
        changed = self.original.replace(
            "Set.GetPrimaryAssetId() != ExpectedSetAssetId",
            "false", 1)
        self.assertFalse(checks(changed)["granted_set_matches_signed_asset_id"])

    def test_aliased_duplicate_uclass_rejected(self):
        changed = self.original.replace(
            "NewAbilityClasses.Contains(AbilityClass)",
            "false", 1)
        self.assertFalse(checks(changed)["resolved_ability_class_has_no_duplicate_or_client_only_execution"])

    def test_abstract_or_deprecated_ability_class_rejected(self):
        changed = self.original.replace(
            "AbilityClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)",
            "false", 1)
        self.assertFalse(checks(changed)["resolved_ability_class_has_no_duplicate_or_client_only_execution"])

    def test_client_only_ability_execution_rejected(self):
        changed = self.original.replace(
            "GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly",
            "GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly", 1)
        self.assertFalse(checks(changed)["resolved_ability_class_has_no_duplicate_or_client_only_execution"])

    def test_cost_execution_not_silently_accepted(self):
        changed = self.original.replace("!CostEffect->Executions.IsEmpty()", "false", 1)
        self.assertFalse(checks(changed)["cost_effect_rejects_hidden_side_effects"])

    def test_cooldown_modifiers_not_silently_accepted(self):
        changed = self.original.replace("!CooldownEffect->Modifiers.IsEmpty()", "false", 1)
        self.assertFalse(checks(changed)["cooldown_effect_rejects_hidden_modifier_and_executions"])

    def test_missing_request_version_check_detected(self):
        self.assertFalse(checks(self.original.replace(
            "const int32 Serial = ++RequestSerial",
            "const int32 Serial = RequestSerial", 1))[
                "old_request_cannot_publish_new_avatar"])

    def test_mismatched_issuer_proof_detected(self):
        self.assertFalse(checks(self.original.replace(
            "CompletedLease.IssuerProof != DefinitionLease.IssuerProof",
            "false", 1))["full_signed_lease"])

    def test_missing_stale_failure_guard_detected(self):
        self.assertFalse(checks(self.original.replace(
            "if (!Set || !ApplyAbilitySet(*Set, Error))\n    {\n        // 给Ability/GE授权的同步通知可触发后继请求；旧失败不应回滚后继成功。\n        if (!IsRequestCurrent()) return;",
            "if (!Set || !ApplyAbilitySet(*Set, Error))\n    {\n        // 模拟缺少请求检查", 1))[
                "stale_failure_cannot_revoke_successor"])

    def test_new_effect_cannot_escape_ledger_detected(self):
        self.assertFalse(checks(self.original.replace(
            "if (IsValid(ASC)) ASC->RemoveActiveGameplayEffect(Handle);",
            "// 故意去掉局部效果撤销", 1))["old_grant_effect_cleanup"])

    def test_new_spec_cannot_escape_ledger_detected(self):
        self.assertFalse(checks(self.original.replace(
            "ASC->CancelAbilityHandle(Handle);\n                ASC->ClearAbility(Handle);",
            "// 故意去掉新Spec撤销", 1))["old_grant_spec_cleanup"])

    def test_lease_reset_must_precede_cancel_callback(self):
        original = self.original
        # 仅修改撤销函数中的第一次重置，避免误命中前面的正常请求分支。
        start = original.index("void UDivineBeastsAbilityLoadoutComponent::RevokeOwnedGrants()")
        edited = original[:start] + original[start:].replace(
            "DefinitionLease = {};", "// 模拟未摘除租约", 1)
        self.assertFalse(checks(edited)["lease_detached_before_external_callbacks"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
