// dedicated translation unit for stb_image_write so the implementation
// lives in exactly one .obj file and the linker doesn't throw a tantrum.
// if you're reading this, yes, stb headers are like that on purpose.

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
