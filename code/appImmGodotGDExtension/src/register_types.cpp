#include "imm_viewer_compositor_effect.h"
#include "imm_viewer_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace godot;

#ifdef _WIN32
static void register_extension_dll_directory()
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&register_extension_dll_directory),
                            &module))
    {
        return;
    }

    wchar_t module_path[MAX_PATH] = {};
    const DWORD path_length = GetModuleFileNameW(module, module_path, MAX_PATH);
    if (path_length == 0 || path_length >= MAX_PATH)
    {
        return;
    }

    wchar_t *last_separator = wcsrchr(module_path, L'\\');
    if (last_separator == nullptr)
    {
        return;
    }

    *last_separator = L'\0';
    AddDllDirectory(module_path);
    SetDllDirectoryW(module_path);
}
#endif

void initialize_imm_godot_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        return;
    }

    ClassDB::register_class<ImmViewerCompositorEffect>();
    ClassDB::register_class<ImmViewerNode>();
}

void uninitialize_imm_godot_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        return;
    }
}

extern "C" GDExtensionBool GDE_EXPORT imm_godot_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
                                                             GDExtensionClassLibraryPtr p_library,
                                                             GDExtensionInitialization *r_initialization)
{
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
#ifdef _WIN32
    register_extension_dll_directory();
#endif
    init_obj.register_initializer(initialize_imm_godot_module);
    init_obj.register_terminator(uninitialize_imm_godot_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
