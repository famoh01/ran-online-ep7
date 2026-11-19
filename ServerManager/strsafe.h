#pragma once
// Compatibility shim: sources include "strsafe.h" (quoted) which used to
// resolve to a bundled VS2003-era strsafe. That old header/lib pair causes
// LNK2019 (__vsnprintf/__vsnwprintf) against modern CRT, so it was removed.
// Forward to the Windows SDK strsafe.h (inline implementations).
#include <strsafe.h>
