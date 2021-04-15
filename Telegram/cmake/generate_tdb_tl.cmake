# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

function(generate_tdb_tl target_name script scheme_file)
    find_package(Python REQUIRED)

    set(gen_dst ${CMAKE_CURRENT_BINARY_DIR}/gen)
    file(MAKE_DIRECTORY ${gen_dst})

    set(gen_timestamp ${gen_dst}/tdb_tl-scheme.timestamp)
    set(gen_files
        ${gen_dst}/tdb_tl-scheme.cpp
        ${gen_dst}/tdb_tl-scheme.h
        ${gen_dst}/tdb_tl-scheme-conversion-from.cpp
        ${gen_dst}/tdb_tl-scheme-conversion-from.h
        ${gen_dst}/tdb_tl-scheme-conversion-to.cpp
        ${gen_dst}/tdb_tl-scheme-conversion-to.h
    )

    add_custom_command(
    OUTPUT
        ${gen_timestamp}
    BYPRODUCTS
        ${gen_files}
    COMMAND
        ${Python_EXECUTABLE}
        ${script}
        -o${gen_dst}/tdb_tl-scheme
        ${scheme_file}
    COMMENT "Generating scheme (${target_name})"
    DEPENDS
        ${script}
        ${submodules_loc}/lib_tl/tl/generate_tl.py
        ${scheme_file}
    )
    generate_target(${target_name} scheme ${gen_timestamp} "${gen_files}" ${gen_dst})
    target_sources(${target_name} PRIVATE ${scheme_file})
endfunction()
