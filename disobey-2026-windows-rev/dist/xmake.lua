add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate")

set_languages("c++20", "c17")
add_requires("libcurl", { configs = { cxflags = "/GS-", vs_runtime = "MT" } })
add_requires("json.h", { configs = { cxflags = "/GS-", vs_runtime = "MT" } })
add_requires("tiny-aes-c", { configs = { cxflags = "/GS-", vs_runtime = "MT" } })

set_optimize("fastest")
set_symbols("debug")
set_strip("all")
set_runtimes("MT")
add_cxflags("/GS-")
add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")

option("mangle_pdbpaths")
    set_description("Make PDB paths point to anime girls instead")
    set_default(false)
    set_showmenu(true)

local PDBPATH_RUNTIME = "https://youtu.be/P1_2JehaKOw" -- How It's Done - Ironmouse & Mori Calliope cover
local PDBPATH_EXECUTABLE = "https://youtu.be/hX_3P0TBGP4" -- Turing Love - Cecilia Immergreen & Gigi Murin cover

target("runtime")
    set_kind("shared")
    add_packages("tiny-aes-c")
    add_files("runtime/**.cpp", "runtime/**.c")
    add_headerfiles("runtime/**.hpp")
    add_includedirs("runtime/", { public = true })
    add_options("mangle_pdbpaths")
    if has_config("mangle_pdbpaths") then
        print("Mangling runtime pdbpaths...")
        add_shflags(("/PDBALTPATH:%s"):format(PDBPATH_RUNTIME))
    end
    add_shflags("/SECTION:INIT,ER")
    add_shflags("/SECTION:MVM,ERW")
    after_build( function (target)
        import("core.base.process")
        local proc = process.open("uv run encrypt_mvm_section.py")
        local ok, status = proc:wait()
        proc:close()
    end)

target("dist")
    set_kind("binary")
    add_deps("runtime")
    add_packages("libcurl", "json.h")
    add_files("src/**.cpp")
    add_ldflags("/MANIFESTUAC:NO")
    add_options("mangle_pdbpaths")
    if has_config("mangle_pdbpaths") then
        print("Mangling dist pdbpaths...")
        add_ldflags(("/PDBALTPATH:%s"):format(PDBPATH_EXECUTABLE), { force = true })
    end
