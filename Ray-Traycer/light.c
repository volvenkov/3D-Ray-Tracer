#include <stdlib.h>
#include <stdio.h>
#include "light.h"

Light* create_light(Vector3 position, Color intensitySpecular, Color intensityDiffuse) {
	Light* light = malloc(sizeof(Light));

	light->position = position;
	light->intensityDiffuse = intensityDiffuse;
	light->intensitySpecular = intensitySpecular;

	return light;
}
