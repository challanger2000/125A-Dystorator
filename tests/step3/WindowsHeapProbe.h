#pragma once
#include <cstdint>
#include <stdexcept>
#if defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#endif

// Test host only. Intercept the process heap underneath both /MT plugin CRT
// and /MD host CRT; global operator new in the host cannot observe DLL heaps.
namespace step3Heap
{
#if defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
// Rtl heap functions also run while the Windows loader initializes a thread,
// before that thread's C++ static TLS is necessarily usable. A reserved TEB
// TLS slot gates observation before any C++ TLS variable is touched by hooks.
inline DWORD captureSlot = TLS_OUT_OF_INDEXES;
inline bool capturing () noexcept
{
    if (captureSlot == TLS_OUT_OF_INDEXES)
        return false;
    const auto savedError = GetLastError ();
    const bool result = TlsGetValue (captureSlot) != nullptr;
    SetLastError (savedError);
    return result;
}
#endif
inline thread_local bool enabled = false;
inline thread_local std::uint64_t allocations = 0, releases = 0;
inline thread_local void* firstCaller = nullptr;
struct Scope
{
    Scope ()
    {
        allocations = releases = 0;
        firstCaller = nullptr;
        enabled = true;
#if defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
        if (!TlsSetValue (captureSlot, reinterpret_cast<void *> (1)))
            throw std::runtime_error ("Cannot arm heap observation; NOT VERIFIED");
#endif
    }
    ~Scope ()
    {
#if defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
        TlsSetValue (captureSlot, nullptr);
#endif
        enabled = false;
    }
};
} // namespace step3Heap
#if defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <intrin.h>
#include <tlhelp32.h>
#include <detours.h>
#include <vector>
namespace step3Heap
{
using Allocate = PVOID (NTAPI *) (PVOID, ULONG, SIZE_T);
using Reallocate = PVOID (NTAPI *) (PVOID, ULONG, PVOID, SIZE_T);
using Release = BOOLEAN (NTAPI *) (PVOID, ULONG, PVOID);
inline Allocate realAllocate = nullptr;
inline Reallocate realReallocate = nullptr;
inline Release realRelease = nullptr;
inline PVOID NTAPI trackedAllocate (PVOID heap, ULONG flags, SIZE_T bytes)
{
    if (capturing ())
    {
        if (allocations == 0) firstCaller = _ReturnAddress();
        ++allocations;
    }
    return realAllocate (heap, flags, bytes);
}
inline PVOID NTAPI trackedReallocate (PVOID heap, ULONG flags, PVOID old, SIZE_T bytes)
{
    if (capturing ())
    {
        if (allocations == 0) firstCaller = _ReturnAddress();
        ++allocations;
    }
    return realReallocate (heap, flags, old, bytes);
}
inline BOOLEAN NTAPI trackedRelease (PVOID heap, ULONG flags, PVOID memory)
{
    if (capturing () && memory != nullptr)
        ++releases;
    return realRelease (heap, flags, memory);
}
class Installation
{
  public:
    explicit Installation (bool requested) : installed (requested)
    {
        if (!requested)
            return;
        const auto ntdll = GetModuleHandleW (L"ntdll.dll");
        realAllocate = reinterpret_cast<Allocate> (GetProcAddress (ntdll, "RtlAllocateHeap"));
        realReallocate = reinterpret_cast<Reallocate> (GetProcAddress (ntdll, "RtlReAllocateHeap"));
        realRelease = reinterpret_cast<Release> (GetProcAddress (ntdll, "RtlFreeHeap"));
        captureSlot = TlsAlloc ();
        // Use the fixed TEB slots, never the separately allocated expansion array.
        if (captureSlot == TLS_OUT_OF_INDEXES || captureSlot >= TLS_MINIMUM_AVAILABLE)
        {
            if (captureSlot != TLS_OUT_OF_INDEXES) TlsFree (captureSlot);
            captureSlot = TLS_OUT_OF_INDEXES;
            throw std::runtime_error ("No fixed TEB slot for heap observation; NOT VERIFIED");
        }
        // Initialize all C++ observation counters before any detour is installed.
        allocations = releases = 0;
        firstCaller = nullptr;
        enabled = false;
        if (!realAllocate || !realReallocate || !realRelease || !transaction (true))
        {
            TlsFree (captureSlot);
            captureSlot = TLS_OUT_OF_INDEXES;
            throw std::runtime_error ("Cannot install Windows heap observation; NOT VERIFIED");
        }
    }
    ~Installation ()
    {
        enabled = false;
        if (installed)
        {
            if (!transaction (false)) std::terminate ();
            TlsFree (captureSlot);
            captureSlot = TLS_OUT_OF_INDEXES;
        }
    }
    static bool selfTest ()
    {
        // Calls through Win32, not our detour directly. Must actually detect
        // allocate/reallocate/free, guarding against an ineffective hook.
        const auto heap = GetProcessHeap ();
        void *p = nullptr;
        bool ok = false;
        {
            Scope capture;
            p = HeapAlloc (heap, 0, 80);
            if (p)
                p = HeapReAlloc (heap, 0, p, 160);
            ok = p && HeapFree (heap, 0, p) && allocations >= 2 && releases >= 1;
        }
        return ok;
    }

  private:
    bool installed = false;
    static bool transaction (bool attach)
    {
        std::vector<HANDLE> threads;
        const auto snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 entry{};
        entry.dwSize = sizeof (entry);
        bool ok = snapshot != INVALID_HANDLE_VALUE && Thread32First (snapshot, &entry);
        while (ok)
        {
            if (entry.th32OwnerProcessID == GetCurrentProcessId () &&
                entry.th32ThreadID != GetCurrentThreadId ())
            {
                auto thread = OpenThread (THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
                                              THREAD_QUERY_INFORMATION,
                                          FALSE, entry.th32ThreadID);
                if (thread)
                {
                    threads.push_back (thread);
                }
            }
            if (!Thread32Next (snapshot, &entry))
                break;
        }
        if (snapshot != INVALID_HANDLE_VALUE)
            CloseHandle (snapshot);
        // Finish our vector/snapshot allocations before suspending any thread.
        if (!ok || DetourTransactionBegin () != NO_ERROR)
        {
            for (auto thread : threads)
                CloseHandle (thread);
            return false;
        }
        for (auto thread : threads)
            if (DetourUpdateThread (thread) != NO_ERROR)
            {
                ok = false;
                break;
            }
        if (ok)
        {
            if (attach)
                ok = DetourAttach (reinterpret_cast<PVOID *> (&realAllocate),
                                   reinterpret_cast<PVOID> (trackedAllocate)) == NO_ERROR &&
                     DetourAttach (reinterpret_cast<PVOID *> (&realReallocate),
                                   reinterpret_cast<PVOID> (trackedReallocate)) == NO_ERROR &&
                     DetourAttach (reinterpret_cast<PVOID *> (&realRelease),
                                   reinterpret_cast<PVOID> (trackedRelease)) == NO_ERROR;
            else
                ok = DetourDetach (reinterpret_cast<PVOID *> (&realAllocate),
                                   reinterpret_cast<PVOID> (trackedAllocate)) == NO_ERROR &&
                     DetourDetach (reinterpret_cast<PVOID *> (&realReallocate),
                                   reinterpret_cast<PVOID> (trackedReallocate)) == NO_ERROR &&
                     DetourDetach (reinterpret_cast<PVOID *> (&realRelease),
                                   reinterpret_cast<PVOID> (trackedRelease)) == NO_ERROR;
        }
        const bool committed = ok && DetourTransactionCommit () == NO_ERROR;
        if (!ok)
            DetourTransactionAbort ();
        for (auto thread : threads)
            CloseHandle (thread);
        return committed;
    }
};
} // namespace step3Heap
#else
namespace step3Heap
{
struct Installation
{
    explicit Installation (bool requested)
    {
#if defined(_WIN32)
        if (requested)
            throw std::runtime_error ("Windows heap probe dependency missing; NOT RUN");
#else
        (void)requested;
#endif
    }
};
} // namespace step3Heap
#endif
