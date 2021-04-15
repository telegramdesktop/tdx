# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

import glob, re, binascii, os, sys

sys.dont_write_bytecode = True
scriptPath = os.path.dirname(os.path.realpath(__file__))
sys.path.append(scriptPath + '/../../../lib_tl/tl')
from generate_tl import generate

generate({
  'namespaces': {
    'global': 'Tdb',
  },
  'prefixes': {
    'type': 'TL',
    'data': 'TLD',
    'id': 'id',
    'construct': 'tl_',
  },
  'types': {
    'typeId': 'uint32',
  },
  'sections': [
  ],

  'skip': [
    'double ? = Double;',
    'string ? = String;',

    'int32 = Int32;',
    'int53 = Int53;',
    'int64 = Int64;',
    'bytes = Bytes;',

    'vector {t:Type} # [ t ] = Vector t;',
  ],
  'builtin': [
    'double',
    'string',
    'int32',
    'int53',
    'int64',
    'bytes',
  ],
  'builtinTemplates': [
    'vector',
  ],
  'builtinInclude': 'tdb/details/tdb_tl_core.h',
  'nullable': [
  ],

  'conversion': {
    'include': 'td/telegram/td_api.h',
    'namespace': 'td::td_api',
    'builtinAdditional': [
      'bool',
    ],
    'builtinIncludeFrom': 'tdb/details/tdb_tl_core_conversion_from.h',
    'builtinIncludeTo': 'tdb/details/tdb_tl_core_conversion_to.h',
  },

})
