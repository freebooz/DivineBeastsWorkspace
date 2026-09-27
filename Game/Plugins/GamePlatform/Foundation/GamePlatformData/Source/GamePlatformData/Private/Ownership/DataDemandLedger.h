#pragma once

#include <map>
#include <set>
#include <string>

namespace GamePlatform::Data
{
/** 进程级加载器使用的纯需求账本，不进行加载、不保存UObject、不创建第二个资产管理器。仅所属游戏线程使用。 */
class FDemandLedger
{
public:
    using FBundleSet = std::set<std::string>;
    /** 首次登记资产与租约需求。空分组仍持有根资产；重复租约拒绝覆盖，以免丢失其他消费者需求。 */
    bool Add(const std::string& AssetId, const std::string& LeaseId, const FBundleSet& RequestedBundles)
    {
        if (AssetId.empty() || LeaseId.empty()) { return false; }
        return Assets[AssetId].emplace(LeaseId, RequestedBundles).second;
    }
    /** 只释放给定租约，重复释放返回false且不影响他人；无剩余需求时移除资产记录。 */
    bool Remove(const std::string& AssetId, const std::string& LeaseId)
    {
        auto Asset = Assets.find(AssetId);
        if (Asset == Assets.end() || Asset->second.erase(LeaseId) == 0) { return false; }
        if (Asset->second.empty()) { Assets.erase(Asset); }
        return true;
    }
    /** 返回所有有效调用者分组并集的独立值，不将空分组误当零租约。 */
    FBundleSet Bundles(const std::string& AssetId) const
    {
        FBundleSet Result;
        const auto Asset = Assets.find(AssetId);
        if (Asset != Assets.end())
            for (const auto& Lease : Asset->second) { Result.insert(Lease.second.begin(), Lease.second.end()); }
        return Result;
    }
    /** 判断是否仍有根资产需求。是否卸载还须由UE层检查外部声明所有权。 */
    bool HasDemand(const std::string& AssetId) const { return Assets.find(AssetId) != Assets.end(); }
private:
    std::map<std::string, std::map<std::string, FBundleSet>> Assets;
};
}
