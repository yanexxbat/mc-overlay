#pragma once
#include "imgui.h"
#include <vector>
#include <cmath>

namespace mcov::depth {
inline thread_local std::vector<float>* tags=nullptr;
// Null bedeutet Bildschirmgrafik (HUD/ESP); positive Werte sind inverse Sichttiefe.
inline void Tag(ImDrawList& d,int first,float distance) {
    if(!tags||!std::isfinite(distance)||distance<=0) return;
    tags->resize(d.VtxBuffer.Size,0.f);
    for(int i=first;i<d.VtxBuffer.Size;++i) (*tags)[i]=1.f/distance;
}
inline void Vertex(ImDrawList& d,int index,float inverseDepth) {
    if(!tags||!std::isfinite(inverseDepth)||inverseDepth<=0) return;
    tags->resize(d.VtxBuffer.Size,0.f); (*tags)[index]=inverseDepth;
}
struct Scope {
    std::vector<float>* previous;
    Scope(std::vector<float>& values):previous(tags) { tags=&values; }
    ~Scope() { tags=previous; }
};
}
