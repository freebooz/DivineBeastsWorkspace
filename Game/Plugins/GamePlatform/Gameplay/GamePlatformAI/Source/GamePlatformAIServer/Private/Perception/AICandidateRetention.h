#pragma once
// 服务器候选容量裁剪：调用方拥有容器/解绑副作用，本规则只做一次O(N log K)有界堆选择；无世界、静态状态或资源所有权。
#include <algorithm>
#include <cstddef>
#include <iterator>
namespace GamePlatformAICandidateRetention
{
/** 随机访问区间中把更优的KeepCount项放在前段，返回待移除段起点；Comparator必须严格有序，不调用外部世界回调。 */
template <typename Iterator, typename Comparator>
Iterator SelectRetained(Iterator Begin, Iterator End, std::size_t KeepCount, Comparator Better)
{
    const auto Count = static_cast<std::size_t>(std::distance(Begin, End));
    if (KeepCount >= Count) { return End; }
    Iterator Removed = Begin + static_cast<typename std::iterator_traits<Iterator>::difference_type>(KeepCount);
    if (KeepCount != 0) { std::partial_sort(Begin, Removed, End, Better); }
    return Removed;
}
}
