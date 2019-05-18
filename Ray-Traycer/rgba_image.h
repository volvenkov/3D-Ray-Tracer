#include <stdint.h>

#define BMP_TYPE 1

#define BMP_RGB 1
#define BMP_RGBA 2

struct Rgba_image {
	int width;
	int height;
	int format;
	uint32_t** RGBA;
};

typedef struct Rgba_image Rgba_image;

Rgba_image* create_rgba(int width, int height, int format);

Rgba_image* read_rgba(char* file_name, int type);

int write_rgba(char* file_name, Rgba_image* rgba_image, int type);

void free_rgba_image(Rgba_image* rgba_image);