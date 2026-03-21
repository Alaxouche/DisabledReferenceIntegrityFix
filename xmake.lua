set_xmakever('3.0.1')
includes('extern/commonlibsse-ng')

set_project('DisabledReferenceIntegrityFix')
set_version('1.1.1')
set_license('GPL-3.0')

set_languages('c++23')
set_warnings('allextra')
set_policy('package.requires_lock', true)
set_toolset('msvc', 'ninja')

add_rules('mode.debug', 'mode.releasedbg', 'mode.release')

option('skyrim_se')
    set_default(false)
    set_showmenu(true)
    set_description('Build for Skyrim Special Edition')
option_end()

option('skyrim_ae')
    set_default(false)
    set_showmenu(true)
    set_description('Build for Skyrim Anniversary Edition')
option_end()

option('skyrim_vr')
    set_default(false)
    set_showmenu(true)
    set_description('Build for Skyrim VR only')
option_end()

if has_config('skyrim_vr') and (has_config('skyrim_se') or has_config('skyrim_ae')) then
    raise('Cannot combine Skyrim VR with SE/AE builds. Enable only one configuration.')
end

target('DisabledReferenceIntegrityFix')
    add_deps('commonlibsse-ng')

    local runtime = 'se_ae'
    if has_config('skyrim_vr') then
        runtime = 'vr'
    elseif has_config('skyrim_ae') and not has_config('skyrim_se') then
        runtime = 'ae'
    elseif has_config('skyrim_se') and not has_config('skyrim_ae') then
        runtime = 'se'
    end

    add_rules('commonlibsse-ng.plugin', {
        name        = 'DisabledReferenceIntegrityFix',
        author      = 'Alaxouche',
        description = 'No description provided.',
        runtime     = runtime,
        options = {
            address_library = true
        }
    })

    add_files('src/**.cpp')
    add_headerfiles('src/**.h')
    
    -- Install INI file alongside DLL
    after_install(function (target)
        local installdir = target:installdir()
        os.cp("$(projectdir)/DisabledReferenceIntegrityFix.ini", path.join(installdir, "SKSE/Plugins/DisabledReferenceIntegrityFix.ini"))
        print("✓ Installed DisabledReferenceIntegrityFix.ini to: " .. path.join(installdir, "SKSE/Plugins/"))
    end)

    add_includedirs(
        'src',
        '$(projectdir)',
        '$(projectdir)/extern/ClibUtil',
        '$(projectdir)/extern/ClibUtil/detail',
        '$(projectdir)/simpleini'
    )

    set_pcxxheader('src/pch.h')

    if has_config('skyrim_vr') then
        add_defines('ENABLE_SKYRIM_VR')
    elseif has_config('skyrim_se') and not has_config('skyrim_ae') then
        add_defines('ENABLE_SKYRIM_SE')
    elseif has_config('skyrim_ae') and not has_config('skyrim_se') then
        add_defines('ENABLE_SKYRIM_AE')
    else
        add_defines('ENABLE_SKYRIM_SE')
        add_defines('ENABLE_SKYRIM_AE')
    end
