// ipc.hpp - named shared-memory channel linking the in-JVM agent to the overlay.
//
// Deliberately uses the "Local\" kernel-object namespace rather than "Global\":
// Global\ requires SeCreateGlobalPrivilege (i.e. an elevated shell). Both the
// JVM and the overlay run as the same user in the same session, so Local\ is
// both sufficient and the only option that works unelevated.
#pragma once

#include <cstdint>
#include <string>

#include "shared_state.hpp"

namespace mcov {

class SharedChannel {
public:
    SharedChannel() = default;
    ~SharedChannel();

    SharedChannel(const SharedChannel&)            = delete;
    SharedChannel& operator=(const SharedChannel&) = delete;

    // Agent side: creates (or re-uses) the mapping and stamps the header.
    // ownerPid is the JVM's PID, which both sides use to derive the name.
    bool CreateWriter(std::uint32_t ownerPid, std::string* error);

    // Overlay side: opens an existing mapping read/write (control flags are
    // written back through it) and validates magic + version.
    bool OpenReader(std::uint32_t ownerPid, std::string* error);

    void Close();

    SharedBlock*       block()       noexcept { return block_; }
    const SharedBlock* block() const noexcept { return block_; }
    bool               valid() const noexcept { return block_ != nullptr; }

    static std::wstring MakeName(std::uint32_t ownerPid);

private:
    void*        mapping_ = nullptr; // HANDLE, kept opaque to avoid <windows.h> here
    SharedBlock* block_   = nullptr;
    bool         owner_   = false;
};

} // namespace mcov
