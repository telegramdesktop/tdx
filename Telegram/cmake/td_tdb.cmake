# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

add_library(td_tdb OBJECT)
init_target(td_tdb)
add_library(tdesktop::td_tdb ALIAS td_tdb)

include(cmake/generate_tdb_tl.cmake)

generate_tdb_tl(td_tdb ${src_loc}/tdb/details/tdb_tl_generate.py ${libs_loc}/td/td/generate/scheme/td_api.tl)

target_precompile_headers(td_tdb PRIVATE ${src_loc}/tdb/tdb_pch.h)
nice_target_sources(td_tdb ${src_loc}
PRIVATE
    tdb/tdb_account.cpp
    tdb/tdb_account.h
    tdb/tdb_request_id.h
    tdb/tdb_resolve_chats.cpp
    tdb/tdb_resolve_chats.h
    tdb/tdb_sender.cpp
    tdb/tdb_sender.h
    tdb/tdb_tl_scheme.h

    tdb/tdb_pch.h

    tdb/details/tdb_tl_core.h
    tdb/details/tdb_tl_core_conversion_from.cpp
    tdb/details/tdb_tl_core_conversion_from.h
    tdb/details/tdb_tl_core_conversion_to.cpp
    tdb/details/tdb_tl_core_conversion_to.h
    tdb/details/tdb_tl_generate.py
    tdb/details/tdb_instance.cpp
    tdb/details/tdb_instance.h
)

target_sources(td_tdb PRIVATE ${libs_loc}/td/td/generate/scheme/td_api.tl)

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
