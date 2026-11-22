/*
 * Minimal no-op OBS module used only in the disposable portable test copies.
 * It satisfies libobs' module ABI so the installer's broken bundled modules
 * (AJAX/Decklink hardware SDKs, multi-source-record) can be replaced without
 * making OBS abort core-module loading.
 *
 * Build: cl /LD /W4 /O2 obs_plugin_stub.c /Fe:stub.dll
 */
#include <stdint.h>

typedef struct obs_module obs_module_t;

static obs_module_t *module_pointer = 0;

#ifdef _WIN32
#define WH_EXPORT __declspec(dllexport)
#else
#define WH_EXPORT __attribute__((visibility("default")))
#endif

WH_EXPORT void obs_module_set_pointer(obs_module_t *module)
{
    module_pointer = module;
}

WH_EXPORT uint32_t obs_module_ver(void)
{
    return 0u; /* accepted: libobs only rejects versions newer than itself */
}

WH_EXPORT int obs_module_load(void)
{
    (void)module_pointer;
    return 1;
}
