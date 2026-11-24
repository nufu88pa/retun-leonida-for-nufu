#include <windows.h>

namespace {
constexpr unsigned initial_value = 0x1234abcd;
__declspec(thread) volatile unsigned marker = initial_value;

DWORD WINAPI check_worker(void*) {
    if (marker != initial_value) return 1;
    marker = 42;
    return marker == 42 ? 0 : 2;
}
}

int main() {
    if (marker != initial_value) return 3;
    marker = 10;
    HANDLE worker = CreateThread(nullptr, 0, check_worker, nullptr, 0, nullptr);
    if (!worker) return 4;
    DWORD code = 0;
    const bool ok = WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0 &&
                    GetExitCodeThread(worker, &code) && code == 0;
    CloseHandle(worker);
    return ok && marker == 10 ? 0 : 5;
}
