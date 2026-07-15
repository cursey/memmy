#include "memmy_context.h"

#if OS_WINDOWS
#include "platform/memmy_backend_win32.h"
#elif OS_MACOS
#include "platform/memmy_backend_darwin.h"
#endif

static THREAD_LOCAL Memmy_Context *memmy_tls_context;

Memmy_Context *Memmy_Context_Get(void)
{
    return memmy_tls_context;
}

void Memmy_Context_Set(Memmy_Context *ctx)
{
    memmy_tls_context = ctx;
}

Memmy_Context *Memmy_Context_Push(Memmy_Context *ctx)
{
    Memmy_Context *old_ctx = memmy_tls_context;
    memmy_tls_context = ctx;
    return old_ctx;
}

void Memmy_Context_Pop(Memmy_Context *old_ctx)
{
    memmy_tls_context = old_ctx;
}

Memmy_Status Memmy_Context_InitDefault(Arena *arena, Memmy_Context *ctx, Memmy_Error *error)
{
    if (arena == 0 || ctx == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing arena or context output"));
        return Memmy_Status_InvalidArgument;
    }

    *ctx = (Memmy_Context){0};
#if OS_WINDOWS
    ctx->backend = Memmy_Win32Backend_Create(arena);
#elif OS_MACOS
    ctx->backend = Memmy_DarwinBackend_Create(arena);
#endif
#if OS_WINDOWS || OS_MACOS
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
#else
    Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("backend"),
                    String8_Lit("default backend is not available"));
    return Memmy_Status_Unsupported;
#endif
}
