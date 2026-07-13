#include "imm_web_decoder.h"

// The exported C ABI is implemented by imm_web_decoder. This translation unit
// provides an executable link target for Emscripten without adding a native
// application main loop.
int main()
{
    return imm_web_schema_version() == IMM_WEB_OUTPUT_SCHEMA_VERSION ? 0 : 1;
}
