# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

add_library(td_tdb OBJECT)
init_target(td_tdb)
add_library(tdesktop::td_tdb ALIAS td_tdb)

include(cmake/generate_tdb_tl.cmake)

generate_tdb_tl(td_tdb ${src_loc}/tdb/details/tdb_tl_generate.py ${src_loc}/tdb/td_api.tl)

target_precompile_headers(td_tdb PRIVATE ${src_loc}/tdb/tdb_pch.h)
nice_target_sources(td_tdb ${src_loc}
PRIVATE
    tdb/tdb_account.cpp
    tdb/tdb_account.h
    tdb/tdb_error.h
    tdb/tdb_file_generator.cpp
    tdb/tdb_file_generator.h
    tdb/tdb_file_proxy.cpp
    tdb/tdb_file_proxy.h
    tdb/tdb_files_downloader.cpp
    tdb/tdb_files_downloader.h
    tdb/tdb_format_phone.cpp
    tdb/tdb_format_phone.h
    tdb/tdb_option.h
    tdb/tdb_options.cpp
    tdb/tdb_options.h
    tdb/tdb_request_id.h
    tdb/tdb_sender.cpp
    tdb/tdb_sender.h
    tdb/tdb_tl_scheme.h
    tdb/tdb_upload_bytes.cpp
    tdb/tdb_upload_bytes.h

    tdb/tdb_pch.h

    tdb/details/tdb_tl_core.h
    tdb/details/tdb_tl_core_conversion_from.cpp
    tdb/details/tdb_tl_core_conversion_from.h
    tdb/details/tdb_tl_core_conversion_to.cpp
    tdb/details/tdb_tl_core_conversion_to.h
    tdb/details/tdb_tl_core_external.h
    tdb/details/tdb_tl_generate.py
    tdb/details/tdb_instance.cpp
    tdb/details/tdb_instance.h
)

target_sources(td_tdb PRIVATE ${src_loc}/tdb/td_api.tl)

target_include_directories(td_tdb
PUBLIC
    ${src_loc}
)

target_link_libraries(td_tdb
PUBLIC
    desktop-app::lib_base
    desktop-app::lib_tl
PRIVATE
    desktop-app::external_td
)
