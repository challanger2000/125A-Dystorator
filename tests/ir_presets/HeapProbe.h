#pragma once
#include <cstdlib>
#include <new>

// Thread-local C++ new/delete probe. Intentionally does not claim to intercept
// direct C malloc/free, OS calls or locks inside external libraries.
namespace heapProbe
{
inline thread_local bool enabled = false;
inline thread_local unsigned allocations = 0, deletions = 0;
} // namespace heapProbe
void *operator new (std::size_t size)
{
    if (heapProbe::enabled)
        ++heapProbe::allocations;
    if (auto *p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc ();
}
void *operator new[] (std::size_t size) { return ::operator new (size); }
void operator delete (void *p) noexcept
{
    if (p && heapProbe::enabled)
        ++heapProbe::deletions;
    std::free (p);
}
void operator delete[] (void *p) noexcept { ::operator delete (p); }
void operator delete (void *p, std::size_t) noexcept { ::operator delete (p); }
void operator delete[] (void *p, std::size_t) noexcept { ::operator delete (p); }
