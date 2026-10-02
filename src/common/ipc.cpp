#include "ipc.hpp"

#include <windows.h>

#include "log.hpp"

namespace mcov {
namespace {

std::string FormatLastError(const char* what) {
    const DWORD code = ::GetLastError();
    char  buffer[512] = {};
    ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     buffer, static_cast<DWORD>(sizeof(buffer)), nullptr);
    std::string message = what;
    message += " failed (0x";
    char hex[16] = {};
    ::wsprintfA(hex, "%08X", code);
    message += hex;
    message += "): ";
    message += buffer;
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) message.pop_back();
    return message;
}

} // namespace

SharedChannel::~SharedChannel() { Close(); }

std::wstring SharedChannel::MakeName(std::uint32_t ownerPid) {
    wchar_t buffer[128] = {};
    ::wsprintfW(buffer, L"Local\\mcov_ipc_v%u_%u", kSharedVersion, ownerPid);
    return buffer;
}

bool SharedChannel::CreateWriter(std::uint32_t ownerPid, std::string* error) {
    Close();

    const std::wstring name = MakeName(ownerPid);

    // A NULL security descriptor gives the mapping the creator's default DACL,
    // which grants the same user full access - exactly the scope we want.
    HANDLE handle = ::CreateFileMappingW(INVALID_HANDLE_VALUE,
                                         nullptr,
                                         PAGE_READWRITE,
                                         0,
                                         static_cast<DWORD>(sizeof(SharedBlock)),
                                         name.c_str());
    if (!handle) {
        if (error) *error = FormatLastError("CreateFileMappingW");
        return false;
    }

    const bool preexisting = (::GetLastError() == ERROR_ALREADY_EXISTS);

    void* view = ::MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBlock));
    if (!view) {
        if (error) *error = FormatLastError("MapViewOfFile");
        ::CloseHandle(handle);
        return false;
    }

    mapping_ = handle;
    block_   = static_cast<SharedBlock*>(view);
    owner_   = true;

    // Freshly created sections are zero-filled by the kernel. If we inherited a
    // section from a previous agent load in this same JVM, reset it by hand so
    // the overlay never sees a half-stale snapshot with a live-looking header.
    if (preexisting) {
        std::memset(block_, 0, sizeof(SharedBlock));
    }

    block_->magic    = kSharedMagic;
    block_->version  = kSharedVersion;
    block_->agentPid = ownerPid;
    block_->sequence.store(0, std::memory_order_relaxed);
    block_->heartbeat.store(0, std::memory_order_relaxed);
    block_->controlFlags.store(kCtrlNone, std::memory_order_relaxed);
    block_->status.store(static_cast<std::uint32_t>(AgentStatus::Booting), std::memory_order_release);

    MCOV_LOG_INFO("IPC writer ready on %S (%zu bytes)", name.c_str(), sizeof(SharedBlock));
    return true;
}

bool SharedChannel::OpenReader(std::uint32_t ownerPid, std::string* error) {
    Close();

    const std::wstring name = MakeName(ownerPid);

    // FILE_MAP_WRITE as well as READ: the overlay writes controlFlags back.
    HANDLE handle = ::OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name.c_str());
    if (!handle) {
        if (error) *error = FormatLastError("OpenFileMappingW (is the agent loaded?)");
        return false;
    }

    void* view = ::MapViewOfFile(handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedBlock));
    if (!view) {
        if (error) *error = FormatLastError("MapViewOfFile");
        ::CloseHandle(handle);
        return false;
    }

    auto* candidate = static_cast<SharedBlock*>(view);
    if (candidate->magic != kSharedMagic || candidate->version != kSharedVersion) {
        if (error) {
            *error = "shared block header mismatch - the loaded agent DLL was built "
                     "against a different shared_state.hpp than this overlay";
        }
        ::UnmapViewOfFile(view);
        ::CloseHandle(handle);
        return false;
    }

    mapping_ = handle;
    block_   = candidate;
    owner_   = false;

    MCOV_LOG_INFO("IPC reader attached to %S", name.c_str());
    return true;
}

void SharedChannel::Close() {
    if (block_) {
        if (owner_) {
            block_->status.store(static_cast<std::uint32_t>(AgentStatus::Detaching),
                                 std::memory_order_release);
        }
        ::UnmapViewOfFile(block_);
        block_ = nullptr;
    }
    if (mapping_) {
        ::CloseHandle(static_cast<HANDLE>(mapping_));
        mapping_ = nullptr;
    }
    owner_ = false;
}

} // namespace mcov
