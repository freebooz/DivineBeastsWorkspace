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
    std::cout << "Cases=" << Cases << " Failed=" << Failed << '\n';
    return Failed ? 1 : 0;
}
