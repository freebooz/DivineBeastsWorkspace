#if defined(PLATFORM_ABILITY_NATIVE_TESTS)
#include "../Grants/AbilityGrantLedger.h"
#include <iostream>
#include <limits>
#include <cstdlib>
using namespace GamePlatformAbilityPolicy;
int main()
{
    int Failures = 0;
    auto Check = [&](bool Condition, const char* Reason) { if (!Condition) { ++Failures; std::cerr << Reason << '\n'; } };
    FGrantLedger Ledger(4);
    Check(Ledger.Begin(1, 1, "sourceA"), "first request admitted");
    Check(!Ledger.Begin(2, 1, "sourceA"), "duplicate source must not double grant");
    Check(!Ledger.Complete(1, 2, true), "stale avatar must not complete");
    Check(Ledger.Complete(1, 1, true), "matching async completion");
    Check(!Ledger.Complete(1, 1, true), "completion at most once");
    Check(Ledger.Begin(2, 1, "sourceB"), "independent caller");
    Check(Ledger.BeginRelease(1, 1), "release invalidates before callbacks");
    Check(!Ledger.Complete(1, 1, true), "synchronous late success cannot resurrect release");
    Check(Ledger.Get(2) == EGrantState::Pending, "other caller unaffected");
    Check(Ledger.FinishRelease(1), "release completion");
    Check(Ledger.BeginRelease(1, 1), "repeated legitimate release is idempotent");
    Check(!Ledger.BeginRelease(1, 99), "modified handle cannot be released");
    Check(Ledger.BeginRelease(2, 1), "cancel pending");
    Check(!Ledger.Complete(2, 1, true), "cancel wins late completion");
    Check(Ledger.FinishRelease(2), "cancel cleanup");
    Check(Ledger.Begin(3, 2, "sourceA"), "new source grant after release");
    Check(!Ledger.Begin(3, 2, "sourceC"), "sequence cannot be reused");
    Check(Ledger.Complete(3, 2, false), "failed grant terminal");
    Check(Ledger.Begin(4, 2, "sourceD"), "last bounded record");
    Check(!Ledger.Begin(5, 2, "sourceE"), "history bound must reject without erasing ownership");
    Check(!Ledger.BeginRelease(100, 2), "forged handle rejected");
    FGrantLedger AnotherWorld(4);
    Check(AnotherWorld.Begin(1, 1, "sourceA"), "independent instance");
    Check(Ledger.Get(1) == EGrantState::Released, "instance isolation");
    Check(!AllowsInput(false, true, 1, 1), "non-owner input");
    Check(!AllowsInput(true, false, 1, 1), "inactive player input");
    Check(!AllowsInput(true, true, 1, 2), "old avatar input");
    Check(AllowsInput(true, true, 2, 2), "current owner input");
    Check(!AllowsInput(true, true, 0, 0), "uninitialized input");
    std::cout << "Assertions=27 Failed=" << Failures << '\n';
    return Failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
