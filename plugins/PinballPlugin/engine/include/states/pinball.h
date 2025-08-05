#ifndef STATE_PINBALL_H
#define STATE_PINBALL_H

#include <gbdk/platform.h>
#include "math.h"

typedef struct circle_t {
	int8_t center_x, center_y;
	uint8_t radius;
} circle_t; //3 bytes

typedef struct line_t {
	int8_t start_x, start_y, end_x, end_y;
} line_t; //4 bytes

typedef struct ball_t {   
	uint8_t actor_idx;
	circle_t circle;
	int16_t vel_x, vel_y;
	uint8_t mass, col_layer;
	point16_t collision_point;
} ball_t; //10 bytes

typedef struct flipper_t {
	uint8_t actor_idx;
	circle_t start_circle, end_circle;
	line_t top_line, bottom_line;
	uint8_t dampen, angle, max_angle, input;
	int8_t rot_speed;
} flipper_t; //20 bytes

typedef struct bumper_circle_t {
	uint8_t actor_idx;
	circle_t circle;
	uint8_t dampen, force;
} bumper_circle_t; //6 bytes

typedef struct bumper_line_t {
	uint8_t actor_idx;
	line_t line;
	uint8_t dampen, force;
} bumper_line_t; //7 bytes

void pinball_init(void) BANKED;
void pinball_update(void) BANKED;

#endif
