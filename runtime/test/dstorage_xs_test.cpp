#include <windows.h>

#include "file_system.h"
#include "shims.h"

int main() {
    using GetFactory = HRESULT (WINAPI*)(REFIID, void**);
    using Resize = HRESULT (WINAPI*)(void*, UINT32);
    using Release = ULONG (WINAPI*)(void*);
    using Open = HRESULT (WINAPI*)(void*, const wchar_t*, REFIID, void**);
    auto get = reinterpret_cast<GetFactory>(rt::shims::find("xcrdapi.dll", "DStorageGetFactory"));
    if (!get) return 1;
    constexpr GUID xbox = {0x62959d11, 0xd45f, 0x4884, {0xa0, 0xa3, 0xf7, 0x86, 0x2e, 0xa2, 0x43, 0xae}};
    constexpr GUID pc = {0x6924ea0c, 0xc3cd, 0x4826, {0xb1, 0x0a, 0xf6, 0x4f, 0x4e, 0xd9, 0x27, 0xc1}};
    constexpr GUID file_iid = {0x43704f9f, 0x27c6, 0x4a9f, {0x88, 0x15, 0xd1, 0x65, 0x27, 0x89, 0x4d, 0xf4}};
    void* first = nullptr;
    void* second = nullptr;
    if (get(xbox, nullptr) != E_POINTER || get(pc, &first) != E_NOINTERFACE || first) return 2;
    if (get(xbox, &first) != S_OK || !first || get(xbox, &second) != S_OK || first != second) return 3;
    const auto* slots = *static_cast<const void* const* const*>(first);
    auto resize = reinterpret_cast<Resize>(const_cast<void*>(slots[8]));
    auto release = reinterpret_cast<Release>(const_cast<void*>(slots[2]));
    if (resize(first, 32 * 1024 * 1024) != S_OK || resize(first, 1) != E_INVALIDARG ||
        resize(first, 0) != S_OK || resize(second, 16 * 1024 * 1024) != S_OK) return 4;
    wchar_t path[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!len || len == MAX_PATH) return 6;
    const std::filesystem::path fixture(path);
    rt::fs::configure(fixture.parent_path(), fixture.parent_path() / L"dstorage_test_overlay");
    const auto console_path = L"G:\\" + fixture.filename().native();
    auto open = reinterpret_cast<Open>(const_cast<void*>(slots[4]));
    void* file = nullptr;
    if (open(first, L"G:\\dstorage_test_missing.fixture", file_iid, &file) != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        file || open(first, console_path.c_str(), file_iid, &file) != S_OK || !file) return 7;
    if (resize(first, 0) != HRESULT_FROM_WIN32(ERROR_BUSY)) return 8;
    const auto* file_slots = *static_cast<const void* const* const*>(file);
    auto get_handle = reinterpret_cast<HANDLE (WINAPI*)(void*)>(const_cast<void*>(file_slots[4]));
    HANDLE handle = get_handle(file);
    LARGE_INTEGER size{};
    if (handle == INVALID_HANDLE_VALUE || !GetFileSizeEx(handle, &size) || size.QuadPart <= 0) return 9;
    CloseHandle(handle);
    auto file_release = reinterpret_cast<Release>(const_cast<void*>(file_slots[2]));
    if (file_release(file) != 0 || resize(first, 0) != S_OK) return 10;
    return release(second) == 1 && release(first) == 0 ? 0 : 5;
}
