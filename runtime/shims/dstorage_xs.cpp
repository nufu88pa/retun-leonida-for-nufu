// Xbox DirectStorage factory surface measured during startup.
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <new>

#include "shims.h"

namespace {

constexpr GUID kFactory = {0x62959d11, 0xd45f, 0x4884, {0xa0, 0xa3, 0xf7, 0x86, 0x2e, 0xa2, 0x43, 0xae}};
constexpr GUID kFile = {0x43704f9f, 0x27c6, 0x4a9f, {0x88, 0x15, 0xd1, 0x65, 0x27, 0x89, 0x4d, 0xf4}};
constexpr GUID kIUnknown = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
constexpr UINT32 kMiB = 1024 * 1024;

struct Factory {
    const void* const* vtable;
    std::atomic<ULONG> refs{0};
    std::mutex mutex;
    void* staging = nullptr;
    UINT32 staging_size = 0;
    unsigned open_files = 0;
    bool initialized = false;

    ~Factory() {
        if (staging) VirtualFree(staging, 0, MEM_RELEASE);
    }

    HRESULT resize(UINT32 size) {
        if (size != 0 && (size < 16 * kMiB || size > 32 * kMiB || size % (4 * kMiB)))
            return E_INVALIDARG;
        if (size == staging_size) return S_OK;
        void* next = size ? VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) : nullptr;
        if (size && !next) return E_OUTOFMEMORY;
        if (staging) VirtualFree(staging, 0, MEM_RELEASE);
        staging = next;
        staging_size = size;
        return S_OK;
    }
};

ULONG WINAPI add_ref(Factory* self) { return ++self->refs; }

ULONG WINAPI release(Factory* self) {
    ULONG count = self->refs.load();
    while (count && !self->refs.compare_exchange_weak(count, count - 1)) {}
    return count ? count - 1 : 0;
}

HRESULT WINAPI query(Factory* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid != kFactory && iid != kIUnknown) return E_NOINTERFACE;
    add_ref(self);
    *out = self;
    return S_OK;
}

template <unsigned Slot>
HRESULT WINAPI unsupported(Factory*) {
    std::fprintf(stderr, "[rt] DirectStorage: factory slot %u is not implemented\n", Slot);
    return E_NOTIMPL;
}

HRESULT WINAPI set_staging_size(Factory* self, UINT32 size) {
    std::lock_guard lock(self->mutex);
    if (self->open_files) return HRESULT_FROM_WIN32(ERROR_BUSY);
    return self->resize(size);
}

struct File {
    const void* const* vtable;
    Factory* factory;
    HANDLE handle;
    std::atomic<ULONG> refs{1};
};

ULONG WINAPI file_add_ref(File* self) { return ++self->refs; }

HRESULT WINAPI file_query(File* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid != kFile && iid != kIUnknown) return E_NOINTERFACE;
    file_add_ref(self);
    *out = self;
    return S_OK;
}

void WINAPI file_close(File* self) {
    std::lock_guard lock(self->factory->mutex);
    if (self->handle == INVALID_HANDLE_VALUE) return;
    CloseHandle(self->handle);
    self->handle = INVALID_HANDLE_VALUE;
}

ULONG WINAPI file_release(File* self) {
    const ULONG remaining = --self->refs;
    if (remaining == 0) {
        file_close(self);
        {
            std::lock_guard lock(self->factory->mutex);
            --self->factory->open_files;
        }
        delete self;
    }
    return remaining;
}

HANDLE WINAPI file_get_handle(File* self) {
    std::lock_guard lock(self->factory->mutex);
    // ReOpenFile provides a separate file pointer and callers own the handle.
    return ReOpenFile(self->handle, GENERIC_READ, FILE_SHARE_READ, 0);
}

const void* const kFileVtable[] = {
    reinterpret_cast<const void*>(&file_query),
    reinterpret_cast<const void*>(&file_add_ref),
    reinterpret_cast<const void*>(&file_release),
    reinterpret_cast<const void*>(&file_close),
    reinterpret_cast<const void*>(&file_get_handle),
};

HRESULT WINAPI open_file(Factory* self, const wchar_t* path, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!path) return E_POINTER;
    if (iid != kFile) return E_NOINTERFACE;
    using Create = decltype(&CreateFileW);
    const auto create = reinterpret_cast<Create>(rt::shims::find("kernel32.dll", "CreateFileW"));
    if (!create) return E_UNEXPECTED;  // console paths must use the existing overlay rules
    const HANDLE handle = create(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    auto* file = new (std::nothrow) File{kFileVtable, self, handle};
    if (!file) {
        CloseHandle(handle);
        return E_OUTOFMEMORY;
    }
    {
        std::lock_guard lock(self->mutex);
        ++self->open_files;
    }
    *out = file;
    return S_OK;
}

const void* const kVtable[] = {
    reinterpret_cast<const void*>(&query),
    reinterpret_cast<const void*>(&add_ref),
    reinterpret_cast<const void*>(&release),
    reinterpret_cast<const void*>(&unsupported<3>),
    reinterpret_cast<const void*>(&open_file),
    reinterpret_cast<const void*>(&unsupported<5>),
    reinterpret_cast<const void*>(&unsupported<6>),
    reinterpret_cast<const void*>(&unsupported<7>),
    reinterpret_cast<const void*>(&set_staging_size),
    reinterpret_cast<const void*>(&unsupported<9>),
    reinterpret_cast<const void*>(&unsupported<10>),
};

HRESULT WINAPI get_factory(REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid != kFactory) return E_NOINTERFACE;
    static Factory factory{kVtable};
    {
        std::lock_guard lock(factory.mutex);
        if (!factory.initialized) {
            const HRESULT result = factory.resize(32 * kMiB);
            if (FAILED(result)) return result;
            factory.initialized = true;
        }
    }
    return query(&factory, iid, out);
}

}  // namespace

SHIM_TABLE(dstorage_xs,
    {"xcrdapi.dll", "DStorageGetFactory", reinterpret_cast<void*>(&get_factory)},
);
