#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "scene_object.h"
#include "ray_tracer.h"
#include "material.h"
#include "ray_hit.h"
#include "color.h"
#include "light.h"
#include "ray.h"

static Color phong_lighting_at_point(Scene* scene, Scene_object* scene_object, Vector3 point, Vector3 normal, Vector3 view);

static Color traced_value_at_pixel(Scene* scene, int width, int height, float dx, float dy, int x, int y, int num_bounces, int num_samples_per_direction);

static Color traced_value_at_position_on_image_plane(Scene* scene, float xt, float yt, int num_bounces);

static Color color_from_ray_hit(Scene* scene, Ray ray, int num_bounces, Scene_object* reflection_to_where);

static Color color_from_any_ray_hit(Scene* scene, Ray ray, int num_bounces);

static Color color_from_reflection_ray_hit(Scene* scene, Ray ray, int num_bounces, Scene_object* reflection_to_where);

static int is_point_in_shadow_from_light(Scene* scene, Scene_object* scene_object, Vector3 point, Light light);

static int num_samples_per_pixel;

void traced_colors(Scene* scene, Color** colors, int width, int height, int num_bounces, int num_samples_per_direction) {
    num_samples_per_pixel = num_samples_per_direction * num_samples_per_direction;

    float dx = 1.0 / (width * num_samples_per_direction);
    float dy = 1.0 / (height * num_samples_per_direction);

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			colors[i][j] = traced_value_at_pixel(scene, width, height, dx, dy, j, i, num_bounces, num_samples_per_direction);
		}
	}
}

static Color traced_value_at_pixel(Scene* scene, int width, int height, float dx, float dy, int x, int y, int num_bounces, int num_samples_per_direction) {
	float xt = (float)x / width;
	float yt = (float)y / height;
    
    Color color = COLOR_BLACK;

    for (int i = 0; i < num_samples_per_direction; i++) {
        for (int j = 0; j < num_samples_per_direction; j++) {
            color = color_plus(color, traced_value_at_position_on_image_plane(scene, xt + dx * i, yt + dy * j, num_bounces));
        }
    }

	return color_clamped(color_divide(color, num_samples_per_pixel));
}


static Color traced_value_at_position_on_image_plane(Scene* scene, float xt, float yt, int num_bounces){
    Vector3 top = vector3_lerp(scene->image_plane.top_left, scene->image_plane.top_right, xt);

	Vector3 bottom = vector3_lerp(scene->image_plane.bottom_left, scene->image_plane.bottom_right, xt);

	Vector3 point = vector3_plus(vector3_lerp(bottom, top, yt), scene->camera);

	Color color = color_from_any_ray_hit(scene, create_ray(point, vector3_minus(point, scene->camera)), num_bounces);

	return color_clamped(color);
}

static Color color_from_ray_hit(Scene* scene, Ray ray, int num_bounces, Scene_object* reflection_to_where) {
	int count_objects = array_list_size(scene->objects);

	if (count_objects <= 0) {
		return COLOR_BLACK;
	}

	Array_list* objects = scene->objects;
	
	Scene_object* scene_object;
	Ray_hit ray_hit; 
	float t;

	ray_hit.t = -1;

	for (int i = 0; i < count_objects; i++) {
		scene_object = array_list_get(objects, i);

        if (scene_object == reflection_to_where) {
            continue;
        }

		t = scene_object->earliest_intersection(scene_object, ray);

        if(t == -1){
            continue;
        }

		if(ray_hit.t == -1 || t < ray_hit.t) {
			ray_hit.scene_object = scene_object;
			ray_hit.t = t;
		}
	}

	if (ray_hit.t == -1) {
		return COLOR_BLACK;
	}

	ray_hit.normalized = ray_hit.scene_object->normal_at(ray_hit.scene_object, ray_at(ray, ray_hit.t));

	Vector3 view = vector3_normalized(vector3_inverted(ray.dir));

	Vector3 point = ray_at(ray, ray_hit.t);

	Color color = phong_lighting_at_point(scene, ray_hit.scene_object, point, ray_hit.normalized, view);;

	if (num_bounces > 0) {
		Vector3 reflection = vector3_minus(vector3_times(vector3_times(ray_hit.normalized, vector3_dot(view, ray_hit.normalized)), 2), view);

		Color reflected_color = color_from_reflection_ray_hit(scene, create_ray(point, reflection), num_bounces - 1, ray_hit.scene_object);
        
		if (color_compare(reflected_color, COLOR_BLACK) == 0) {
			color = color_plus(color, color_times_c(reflected_color, ray_hit.scene_object->material.kReflection));
		}
	}

	return color;
}

static Color color_from_any_ray_hit(Scene* scene, Ray ray, int num_bounces){
    return color_from_ray_hit(scene, ray, num_bounces, NULL);
}

static Color color_from_reflection_ray_hit(Scene* scene, Ray ray, int num_bounces, Scene_object* reflection_to_where){
    return color_from_ray_hit(scene, ray, num_bounces, reflection_to_where);
}

static Color phong_lighting_at_point(Scene* scene, Scene_object* scene_object, Vector3 point, Vector3 normal, Vector3 view) {
	Light* light;
	Color light_contributions = COLOR_BLACK;
	Vector3 l, r;
	Color diffuse, specular;
    int lights_count = array_list_size(scene->lights);

	for (int i = 0; i < lights_count; i++) {
		light = array_list_get(scene->lights, i);

		if (vector3_dot(vector3_minus(light->position, point), normal) < 0 || is_point_in_shadow_from_light(scene, scene_object, point, *light) == 1) {
			continue;
		}

		l = vector3_normalized(vector3_minus(light->position, point));

		r = vector3_minus(vector3_times(normal, (vector3_dot(l, normal) * 2)), l);

		diffuse = color_times(color_times_c(light->intensityDiffuse, scene_object->material.kDiffuse), vector3_dot(l, normal));

		specular = color_times(color_times_c(light->intensitySpecular, scene_object->material.kSpecular), pow(vector3_dot(r, view), scene_object->material.alpha));

		light_contributions = color_plus(light_contributions, color_plus(diffuse, specular));
	}

	return color_clamped(color_plus(light_contributions, color_times_c(scene_object->material.kAmbient, scene->kAmbientLight)));
}

static int is_point_in_shadow_from_light(Scene* scene, Scene_object* scene_object, Vector3 point, Light light) {
	Vector3 direction = vector3_minus(light.position, point);

	Ray shadow_ray;
	shadow_ray.dir = direction;
	shadow_ray.origin = point;

	Scene_object* temp;
    int count_objects = array_list_size(scene->objects);
    float t;

	for (int i = 0; i < count_objects; i++) {
		temp = array_list_get(scene->objects, i);

		if (temp == scene_object) {
			continue;
		}
        
        t = temp->earliest_intersection(temp, shadow_ray);
        
		if (t != -1 && t <= 1) {
			return 1;
		}
	}

	return 0;
}
