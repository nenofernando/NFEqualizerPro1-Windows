#include "AllocationGuard.h"
#include <cstdlib>
#include <new>
#include <cstddef>

namespace NFTests
{
thread_local bool gGuardActive = false;
thread_local bool gAllocationSeenInGuard = false;
}

void* operator new(std::size_t size)
{
    if (NFTests::gGuardActive)
        NFTests::gAllocationSeenInGuard = true;

    if (size == 0)
        size = 1;

    if (auto* p = std::malloc(size))
        return p;

    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
