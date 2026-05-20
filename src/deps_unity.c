#define SQLITE_ENABLE_FTS5 1
#include "sqlite3.c"

#include "glad.c"
#include "sha2.c"
#include "tinyfiledialogs.c"

// STB libs
#define STBI_WINDOWS_UTF8 1
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
