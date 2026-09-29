# JUCE's ICON_BIG/ICON_SMALL supply Windows resources and macOS AU/VST3/app
# bundle icons. Preserve the user's original 15-size ICO on Windows.
set(vc_branding "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Branding")
if(WIN32)
    get_target_property(vc_generated_icon VocalCompanion JUCE_ICON_FILE)
    if(vc_generated_icon)
        configure_file("${vc_branding}/VocalCompanion.ico" "${vc_generated_icon}" COPYONLY)
    endif()
    # The CLAP extension does not attach JUCE's Windows resource library.
    if(TARGET VocalCompanion_CLAP AND TARGET VocalCompanion_rc_lib)
        target_link_libraries(VocalCompanion_CLAP PRIVATE VocalCompanion_rc_lib)
    endif()
    # Folder attributes do not survive ZIP extraction. Reapply at configure.
    execute_process(COMMAND attrib +r "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND attrib +s +h "${CMAKE_CURRENT_SOURCE_DIR}/desktop.ini" OUTPUT_QUIET ERROR_QUIET)
endif()

if(APPLE AND TARGET VocalCompanion_CLAP)
    # The extension's plist already names clap.icns. Replace its generic image
    # after its own resource copy, then refresh its installed copy if enabled.
    get_target_property(vc_mac_icon VocalCompanion JUCE_ICON_FILE)
    if(vc_mac_icon)
        add_custom_command(TARGET VocalCompanion_CLAP POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${vc_mac_icon}"
                "$<TARGET_BUNDLE_DIR:VocalCompanion_CLAP>/Contents/Resources/clap.icns"
            VERBATIM)
        get_target_property(vc_copy_plugins VocalCompanion JUCE_COPY_PLUGIN_AFTER_BUILD)
        if(vc_copy_plugins)
            add_custom_command(TARGET VocalCompanion_CLAP POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "$<TARGET_BUNDLE_DIR:VocalCompanion_CLAP>"
                    "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP/Vocal Companion.clap"
                VERBATIM)
        endif()
    endif()
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    # ELF files have no Explorer-style embedded icon. Desktop launchers and
    # the icon theme provide the application-menu icon; JUCE sets the window icon.
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/VocalCompanion_artefacts/$<CONFIG>/Vocal Companion.desktop"
        CONTENT "[Desktop Entry]\nType=Application\nName=Vocal Companion\nComment=Modular vocal effects by Crimson Redstone\nExec=\"$<TARGET_FILE:VocalCompanion_Standalone>\"\nIcon=${vc_branding}/VocalCompanion-256.png\nTerminal=false\nCategories=AudioVideo;Audio;\n")
    install(PROGRAMS "$<TARGET_FILE:VocalCompanion_Standalone>"
        DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME vocal-companion COMPONENT Standalone)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Branding/com.crimsonredstone.vocalcompanion.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT Standalone)
    foreach(vc_icon_size 16 32 48 64 128 256)
        install(FILES "${vc_branding}/VocalCompanion-${vc_icon_size}.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${vc_icon_size}x${vc_icon_size}/apps"
            RENAME com.crimsonredstone.vocalcompanion.png COMPONENT Standalone)
    endforeach()
endif()
