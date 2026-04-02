add_requires("phnt", "wil")

option("mangle_pdbpath")
set_description("Make PDB path of vulnerable driver point to anime girls")
set_default(false)

-- Note: you probably don't need to install new cert to certstore - or at least I didn't have to to load the drivers
--
-- Candidate videos
local MISS_RGB = "https://youtu.be/haRmoe9WgNM"
local GUESS = "https://youtu.be/w2P3JKcM9gc"

target("RgbController")
set_languages("c++latest")
set_exceptions("no-cxx")
set_strip("all")
set_symbols("debug")
set_optimize("fastest")
add_rules("wdk.driver", "wdk.env.wdm")
add_files("vulnerable/**.cpp")
add_headerfiles("vulnerable/**.hpp")
add_includedirs("vulnerable")
add_defines("WIL_SUPPRESS_EXCEPTIONS")
add_packages("wil")
set_values("wdk.sign.mode", "test")
set_values("wdk.sign.store", "PrivateCertStore")
set_values("wdk.sign.company", "mvm industries")
set_values("wdk.sign.digest_algorithm", "sha256")
if has_config("mangle_pdbpath") then
    add_ldflags(("/PDBALTPATH:%s"):format(MISS_RGB), { force = true })
end

target("Client")
set_kind("binary")
set_runtimes("MT")
set_languages("c++latest")
set_strip("all")
set_symbols("debug")
add_files("client/**.cpp")
add_headerfiles("client/**.hpp")
add_includedirs("client")

target("ChallengeBinary")
set_kind("binary")
set_runtimes("MT")
set_languages("c17")
set_strip("all")
set_symbols("debug")
set_optimize("fastest")
add_packages("phnt")
add_files("target_binary/**.c")
add_headerfiles("target_binary/**.h")
add_includedirs("target_binary")
add_syslinks("User32")
if has_config("mangle_pdbpath") then
    add_ldflags(("/PDBALTPATH:%s"):format(GUESS), { force = true })
end

target("AnticheatDriver")
set_languages("c++latest")
set_exceptions("no-cxx")
set_strip("all")
set_symbols("debug")
set_optimize("fastest")
add_rules("wdk.driver", "wdk.env.wdm")
add_files("anticheat/**.cpp")
add_headerfiles("anticheat/**.hpp")
add_includedirs("anticheat")
add_defines("WIL_SUPPRESS_EXCEPTIONS")
add_packages("phnt", "wil")
add_ldflags("/INTEGRITYCHECK", { force = true })
set_values("wdk.sign.mode", "test")
set_values("wdk.sign.store", "PrivateCertStore")
set_values("wdk.sign.company", "mvm industries")
set_values("wdk.sign.digest_algorithm", "sha256")
