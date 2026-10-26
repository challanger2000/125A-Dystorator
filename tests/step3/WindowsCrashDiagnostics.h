#pragma once
// Fatal diagnostics for the QA executables only. No handler in the plugin.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>
namespace step3Crash
{
inline void stack ()
{
    void *frames[48]{};
    const auto count = CaptureStackBackTrace (0, 48, frames, nullptr);
    const auto process = GetCurrentProcess ();
    SymSetOptions (SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    const bool symbols = SymInitialize (process, nullptr, TRUE) != FALSE;
    for (USHORT i = 0; i < count; ++i)
    {
        alignas (SYMBOL_INFO) char storage[sizeof (SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto *symbol = reinterpret_cast<SYMBOL_INFO *> (storage);
        symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        HMODULE module = nullptr;
        wchar_t file[MAX_PATH]{};
        GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR> (frames[i]), &module);
        GetModuleFileNameW (module, file, MAX_PATH);
        const bool named = symbols && SymFromAddr (process, (DWORD64)frames[i], &displacement, symbol);
        std::fprintf (stderr, "STACK %u %p module=%ls offset=0x%llx symbol=%s+0x%llx\n", i, frames[i], file,
                      (unsigned long long)((uintptr_t)frames[i] - (uintptr_t)module),
                      named ? symbol->Name : "?", (unsigned long long)displacement);
    }
    std::fflush (stderr);
}
inline LONG WINAPI exception (EXCEPTION_POINTERS *p)
{
    std::fprintf (stderr, "FAIL Windows unhandled SEH 0x%08lx address=%p\n",
                  p->ExceptionRecord->ExceptionCode, p->ExceptionRecord->ExceptionAddress);
    stack ();
    return EXCEPTION_EXECUTE_HANDLER;
}
inline void install ()
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter (exception);
    _set_invalid_parameter_handler (
        [] (const wchar_t *expression, const wchar_t *function, const wchar_t *file, unsigned line, uintptr_t)
        {
            std::fprintf (stderr, "FAIL CRT invalid parameter: %ls function=%ls file=%ls:%u\n",
                          expression ? expression : L"?", function ? function : L"?", file ? file : L"?",
                          line);
            stack ();
            std::_Exit (2);
        });
    std::set_terminate (
        []
        {
            if (auto error = std::current_exception ())
                try
                {
                    std::rethrow_exception (error);
                }
                catch (const std::exception &e)
                {
                    std::fprintf (stderr, "FAIL uncaught C++ exception: %s\n", e.what ());
                }
                catch (...)
                {
                    std::fprintf (stderr, "FAIL uncaught unknown C++ exception\n");
                }
            else
                std::fprintf (stderr, "FAIL terminate without active exception\n");
            stack ();
            std::_Exit (2);
        });
}
} // namespace step3Crash
#else
namespace step3Crash
{
inline void install () {}
} // namespace step3Crash
#endif
