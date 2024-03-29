if(CRASHPAD_ZLIB_SYSTEM)
    add_library(crashpad_zlib INTERFACE)
    target_compile_definitions(crashpad_zlib INTERFACE
        ZLIB_CONST
        CRASHPAD_ZLIB_SOURCE_SYSTEM
        $<BUILD_INTERFACE:${ZLIB_COMPILE_DEFINITIONS}>
    )
    target_link_libraries(crashpad_zlib INTERFACE ZLIB::ZLIB)
else()
    add_library(crashpad_zlib STATIC
        zlib/adler32.c
        zlib/compress.c
        zlib/crc32.c
        zlib/crc32.h
        zlib/deflate.c
        zlib/deflate.h
        zlib/gzclose.c
        zlib/gzguts.h
        zlib/gzlib.c
        zlib/gzread.c
        zlib/gzwrite.c
        zlib/infback.c
        zlib/inffast.c
        zlib/inffast.h
        zlib/inffixed.h
        zlib/inflate.c
        zlib/inflate.h
        zlib/inftrees.c
        zlib/inftrees.h
        zlib/trees.c
        zlib/trees.h
        zlib/uncompr.c
        zlib/zconf.h
        zlib/zlib.h
        zlib/zutil.c
        zlib/zutil.h
        zlib_crashpad.h
    )
    if (APPLE)
        get_property(archs TARGET crashpad_zlib PROPERTY OSX_ARCHITECTURES)
    endif()
    if(NOT archs)
        set(archs ${CMAKE_SYSTEM_PROCESSOR})
    endif()
    if(archs MATCHES "(x86_64)|(x86)|(i[3-7]86)|(AMD64)")
        target_sources(crashpad_zlib PRIVATE
            zlib/crc_folding.c
        )

        if(NOT MSVC)
            target_compile_options(crashpad_zlib PRIVATE -msse4.2 -mpclmul)
        endif()
    endif()
    target_compile_definitions(crashpad_zlib PUBLIC
        CRASHPAD_ZLIB_SOURCE_EMBEDDED
    )
    target_compile_definitions(crashpad_zlib
        PUBLIC
            ZLIB_CONST
        PRIVATE
            HAVE_STDARG_H
    )
    target_include_directories(crashpad_zlib PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zlib>"
    )
    target_link_libraries(crashpad_zlib PRIVATE
        $<BUILD_INTERFACE:crashpad_interface>
    )

    if(MSVC)
        target_compile_options(crashpad_zlib PRIVATE
            "/wd4131" # uses old-style declarator
            "/wd4244" # conversion from 't1' to 't2', possible loss of data
            "/wd4245" # conversion from 't1' to 't2', signed/unsigned mismatch
            "/wd4267" # conversion from 'size_t' to 't', possible loss of data
            "/wd4324" # structure was padded due to alignment specifier
            "/wd4702" # unreachable code
            "/wd5105" # see https://github.com/getsentry/sentry-native/issues/415
        )
    endif()
endif()

set_property(TARGET crashpad_zlib PROPERTY EXPORT_NAME zlib)
add_library(crashpad::zlib ALIAS crashpad_zlib)

crashpad_install_target(crashpad_zlib)
