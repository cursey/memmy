#include "memmy_backend.h"

B32 Memmy_Backend_HasCapability(Memmy_Backend *backend, Memmy_BackendCap capability)
{
    return backend != 0 && (backend->capabilities & capability) == capability;
}
