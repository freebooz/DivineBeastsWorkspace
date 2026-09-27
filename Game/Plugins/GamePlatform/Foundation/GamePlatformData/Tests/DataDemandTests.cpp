#include "Ownership/DataDemandLedger.h"
#include <iostream>

int main()
{
    using GamePlatform::Data::FDemandLedger;
    FDemandLedger Ledger;
    int Failed = 0, Cases = 0;
    auto Check = [&](bool Condition, const char* Name) { ++Cases; if (!Condition) { ++Failed; std::cerr << Name << '\n'; } };
    Check(Ledger.Add("D", "ScopeA.Lease1", {"Core", "UI", "Core"}), "AddA");
    Check(Ledger.Add("D", "ScopeB.Lease1", {"Core", "Server"}), "AddB");
    Check(Ledger.Bundles("D") == FDemandLedger::FBundleSet{"Core", "UI", "Server"}, "UnionPreservesBothScopes");
    Check(!Ledger.Add("D", "ScopeA.Lease1", {"Other"}), "DuplicateLeaseMustNotOverwrite");
    Check(Ledger.Remove("D", "ScopeA.Lease1"), "ReleaseA");
    Check(Ledger.Bundles("D") == FDemandLedger::FBundleSet{"Core", "Server"}, "BStillOwnsRequiredBundles");
    Check(Ledger.HasDemand("D"), "BPreventsUnload");
    Check(!Ledger.Remove("D", "ScopeA.Lease1") && Ledger.HasDemand("D"), "RepeatedReleaseDoesNotAffectB");
    Check(Ledger.Remove("D", "ScopeB.Lease1") && !Ledger.HasDemand("D"), "LastReleaseMayUnload");
    Check(!Ledger.Add("", "L", {}) && !Ledger.Add("D", "", {}), "InvalidIdentityRejected");
    Check(Ledger.Add("RootOnly", "C", {}) && Ledger.HasDemand("RootOnly"), "EmptyBundlesStillHoldRoot");
    Check(Ledger.Bundles("Missing").empty() && !Ledger.HasDemand("Missing"), "ReadDoesNotCreateDemand");
    // 主租约同时持有根和依赖；撤销其中一个资产不能减少另一个资产或其他实例的需求。
    Check(Ledger.Add("Root", "ScopeA.Graph1", {"Core", "UI"}) &&
          Ledger.Add("Dependency", "ScopeA.Graph1", {"Core", "UI"}), "OneLeaseCanOwnMultipleAssets");
    Check(Ledger.Add("Dependency", "ScopeB.Graph2", {"Core", "Server"}), "SharedDependencyHasIndependentOwner");
    Check(Ledger.Remove("Root", "ScopeA.Graph1") && Ledger.HasDemand("Dependency"), "RootReleaseDoesNotRemoveDependencyImplicitly");
    Check(Ledger.Remove("Dependency", "ScopeA.Graph1") &&
          Ledger.Bundles("Dependency") == FDemandLedger::FBundleSet{"Core", "Server"}, "GraphRollbackPreservesOtherScope");
    Check(!Ledger.Remove("Dependency", "ScopeA.Unknown") && Ledger.HasDemand("Dependency"), "UnknownOwnerCannotReleaseOtherScope");
    Check(Ledger.Remove("Dependency", "ScopeB.Graph2") && !Ledger.HasDemand("Dependency"), "LastDependencyOwnerLeavesNoDemand");
    Check(Ledger.Remove("RootOnly", "C") && !Ledger.HasDemand("RootOnly"), "EmptyBundleLeaseIsReleasedExplicitly");
    Check(Ledger.Add("D", "ScopeC.NewGeneration", {"New"}) &&
          !Ledger.Remove("D", "ScopeA.Lease1") && Ledger.Bundles("D") == FDemandLedger::FBundleSet{"New"}, "StaleLeaseCannotTouchNewDemand");
    std::cout << "Cases=" << Cases << " Failed=" << Failed << '\n';
    return Failed ? 1 : 0;
}
