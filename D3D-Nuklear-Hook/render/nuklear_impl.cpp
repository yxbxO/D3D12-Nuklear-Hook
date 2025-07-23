#pragma once

#pragma warning(push)
#pragma warning(disable: 4305) // truncation from 'nk_window_flags' to 'nk_bool'
#pragma warning(disable: 4805) // unsafe mix of type 'int' and type 'nk_bool'
#pragma warning(disable: 4244) // conversion from 'SIZE_T' to 'int', possible loss of data

#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_ASSERT



#define NK_IMPLEMENTATION
#include "Nuklear/nuklear.h"