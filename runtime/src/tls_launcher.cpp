// Keep runtime TLS in gta6rt_core.dll. The EXE's slot holds only this
// anchor so the loader can lend it to a mapped executable.
__declspec(thread) volatile unsigned char tls_anchor = 0;
extern "C" unsigned long _tls_index;
extern "C" __declspec(dllimport) int gta6rt_main(int argc, wchar_t** argv);

extern "C" __declspec(dllexport) unsigned long gta6rt_launcher_tls_index() {
    return _tls_index;
}

int wmain(int argc, wchar_t** argv) {
    tls_anchor = 1;
    return gta6rt_main(argc, argv);
}
