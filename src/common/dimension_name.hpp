#pragma once
#include <algorithm>
#include <cstddef>
#include <string_view>

namespace mcov {
// ResourceKey.toString() is longer than the IPC dimension field. Extract its
// identifier before copying so Overworld, Nether and End remain distinguishable.
inline std::string_view DimensionName(std::string_view value) {
    if(value.starts_with("ResourceKey[")&&value.ends_with(']')) {
        const auto separator=value.rfind(" / ");
        if(separator!=std::string_view::npos) return value.substr(separator+3,value.size()-separator-4);
    }
    return value;
}
template<std::size_t N> inline void CopyDimensionName(char (&destination)[N],std::string_view value) {
    const auto name=DimensionName(value); const auto length=std::min(name.size(),N-1);
    std::copy_n(name.data(),length,destination); destination[length]='\0';
}
} // namespace mcov
