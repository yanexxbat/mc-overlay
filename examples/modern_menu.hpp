#pragma once
#include "../src/common/compact_style.hpp"
#include "../src/overlay/control_panel.hpp"

// Integration adapter for the actual reference-led mc-overlay menu.
// The complete rendering implementation lives in src/overlay/control_panel.cpp.
namespace mcov::menu_example {
using MenuState=control_panel::State;
using Status=control_panel::Status;
using Actions=control_panel::Actions;
using Content=control_panel::Content;
using Catalog=control_panel::Catalog;
using Activate=control_panel::Activate;
inline void ApplyCustomStyle() { style::ApplyCustomStyle(); }
inline Actions RenderMenu(MenuState& state,const Status& status,void* owner,Content content,
                          Catalog catalog=nullptr,Activate activate=nullptr) {
    return control_panel::RenderMenu(state,status,owner,content,catalog,activate);
}
} // namespace mcov::menu_example
