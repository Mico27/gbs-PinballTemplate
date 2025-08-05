#pragma bank 255

#include "data/states_defines.h"
#include "states/pinball.h"

#include <string.h>
#include <stdlib.h>
#include <rand.h>
#include <gbdk/platform.h>

#include "system.h"
#include "vm.h"
#include "math.h"
#include "gbs_types.h"
#include "bankdata.h"
#include "data_manager.h"
#include "actor.h"
#include "game_time.h"
#include "camera.h"
#include "trigger.h"
#include "input.h"
#include "data/game_globals.h"

#define MAX_BALLS 4
#define MAX_FLIPPERS 4
#define MAX_BUMPER_CIRCLES 4
#define MAX_BUMPER_LINES 4

#define DIST2(x1, y1, x2, y2) ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1))
#define DIST(x1, y1, x2, y2)(isqrt(DIST2(x1, y1, x2, y2)))
#define DOT(velx, normalx, vely, normaly) (velx * normalx + vely * normaly)

background_t collision_bkg;
far_ptr_t collision_data;

ball_t balls[MAX_BALLS];
flipper_t flippers[MAX_FLIPPERS];
bumper_circle_t bumper_circles[MAX_BUMPER_CIRCLES];
bumper_line_t bumper_lines[MAX_BUMPER_LINES];

UBYTE actor_behavior_ids[MAX_ACTORS];

uint8_t balls_len;
uint8_t flippers_len;
uint8_t bumper_circles_len;
uint8_t bumper_lines_len;

int16_t ball_grav;
int16_t ball_max_vel;

uint16_t aggregate_x;
uint16_t aggregate_y;
uint8_t ball_nbr;

UBYTE get_collision(int16_t ball_pos_x, int16_t ball_pos_y) BANKED {	
	if (collision_bkg){
		uint8_t pixel_data = ((SUBPX_TO_PX(ball_pos_y) & 7) << 1);
		uint8_t px_x = SUBPX_TO_PX(ball_pos_x) & 7;
		uint16_t tile_offset = (SUBPX_TO_TILE(ball_pos_y) * (UINT16)collision_bkg.width) + SUBPX_TO_TILE(ball_pos_x);
		const tileset_t* tileset_ptr = collision_bkg.tileset.ptr;
		UBYTE * tileset_data = tileset_ptr->tiles;
		unsigned char* tilemap_ptr = collision_bkg.tilemap.ptr;
		unsigned char *collision_data_ptr = collision_data.ptr;
		uint8_t tile_id = ReadBankedUBYTE(tilemap_ptr + tile_offset, collision_bkg.tilemap.bank);
		UBYTE collision_result = ReadBankedUBYTE(collision_data_ptr + tile_offset, collision_data.bank);
		if (collision_result){
			if ((collision_result & 7) == 0){
				return collision_result;
			}
			#ifdef CGB
				if (_is_CGB) {	
					unsigned char* tilemap_attr_ptr = collision_bkg.cgb_tilemap_attr.ptr;		
					const tileset_t* cgb_tileset = collision_bkg.cgb_tileset.ptr;	
					UBYTE * cgb_tileset_data = cgb_tileset->tiles;				
					uint8_t tile_attr_id = ReadBankedUBYTE(tilemap_attr_ptr + tile_offset, collision_bkg.cgb_tilemap_attr.bank);				
					if (tile_attr_id & 0x08 && cgb_tileset_data){
						pixel_data = (ReadBankedUBYTE(cgb_tileset_data + (tile_id << 4) + pixel_data, collision_bkg.cgb_tileset.bank) & (1 << (7 - px_x)));
					} else {
						pixel_data = (ReadBankedUBYTE(tileset_data + (tile_id << 4) + pixel_data, collision_bkg.tileset.bank) & (1 << (7 - px_x)));
					}
					if (pixel_data){
						return collision_result;
					}
					return 0;
					
				} else {
					pixel_data = (ReadBankedUBYTE(tileset_data + (tile_id << 4) + pixel_data, collision_bkg.tileset.bank) & (1 << (7 - px_x)));
					if (pixel_data){
						return collision_result;
					}
					return 0;
				}					
			#else		
				pixel_data = (ReadBankedUBYTE(tileset_data + (tile_id << 4) + pixel_data, collision_bkg.tileset.bank) & (1 << (7 - px_x)));
				if (pixel_data){				
					return collision_result;
				}
				return 0;				
			#endif
		}
	}
	return 0;
}

UBYTE lineCircle(int16_t line_start_x, int16_t line_start_y, int16_t line_end_x, int16_t line_end_y, int16_t circle_center_x, int16_t circle_center_y, int16_t circle_radius, point16_t closest) BANKED
{
	//if the circle is within the boundaries of the line
	if (circle_center_x + circle_radius < MIN(line_start_x, line_end_x))
		return 0;
	if (circle_center_x - circle_radius > MAX(line_start_x, line_end_x))
		return 0;
	if (circle_center_y + circle_radius < MIN(line_start_y, line_end_y))
		return 0;
	if (circle_center_y - circle_radius > MAX(line_start_y, line_end_y))
		return 0;
	
	//use length squared instead of length because the dot product is divided by the squared length anyway
	//this saves having to do a costly square root calculation
	int16_t len2 = DIST2(line_start_x, line_start_y, line_end_x, line_end_y) >> 6;
	int16_t dot = ((((circle_center_x - line_start_x) * (line_end_x - line_start_x)) + ((circle_center_y - line_start_y) * (line_end_y - line_start_y)))) >> 6;
	//find the closest point along the line's infinite plane to the circle's center
	closest.x = line_start_x + ((line_end_x - line_start_x) * dot) / len2;
	closest.y = line_start_y + ((line_end_y - line_start_y) * dot) / len2;
	//since closest point is already on the line's infinite plane we don't need proper line-point collision checking
	//instead we just need to check if the closest point is within the bounds of the line
	//we check both x and y in case of a completely horizontal or vertical line one would always return true
	
	//((circle_center_x - line_start_x) << 4) / (line_end_x - line_start_x)
	
	script_memory[VAR_DISTANCE] = len2;
	script_memory[VAR_DOT] = dot;
	//script_memory[VAR_PIXELX] = closest.x;
	//script_memory[VAR_PIXELY] = closest.y;
	
	//finally check if the distance between the closest point and the circle's center is less than or equal to the circle's radius
	//We use the distance squared and compare it to the radius squared to eliminate a costly square root calculation
	if (DIST2(circle_center_x, circle_center_y, closest.x, closest.y) <= (circle_radius * circle_radius) + 1){
		script_memory[VAR_BALLX] = circle_center_x;
		script_memory[VAR_BALLY] = circle_center_y;	
		
		return 1;
	}		
	return 0;
}

UBYTE circleCircle(int16_t circle1_center_x, int16_t circle1_center_y, int16_t circle1_radius, int16_t circle2_center_x, int16_t circle2_center_y, int16_t circle2_radius, point16_t closest) BANKED
{
	//avoid square root calculation for checking collision
	uint16_t distance = DIST2(circle1_center_x, circle1_center_y, circle2_center_x, circle2_center_y);
	if (distance <= (circle1_radius + circle2_radius) * (circle1_radius + circle2_radius))
	{
		//only calculate the square root if the two circles are in fact colliding
		//this is needed so we can determine the point along the first circle where the collision took place.
		distance = isqrt(distance);
		closest.x = circle1_center_x + ((circle1_center_x - circle2_center_x) / distance) * circle1_radius;
		closest.y = circle1_center_y + ((circle1_center_y - circle2_center_y) / distance) * circle1_radius;
		return 1;
	}
	return 0;
}

void rotatePoint(int8_t target_x, int8_t target_y, int8_t origin_x, int8_t origin_y, int8_t cosine, int8_t sine, point16_t* rotated_point) BANKED {
	
	script_memory[VAR_SINE] = sine;
	script_memory[VAR_COSINE] = cosine;
	rotated_point->x = (target_x - origin_x);
	rotated_point->y = (target_y - origin_y);	
	int16_t tmp = ((rotated_point->x * cosine) - (rotated_point->y * sine)) >> 7;
	script_memory[VAR_TMP] = tmp;
	rotated_point->y = ((rotated_point->x * sine) + (rotated_point->y * cosine)) >> 7;
	rotated_point->x = tmp;	
	rotated_point->x += origin_x;
	rotated_point->y += origin_y;
	script_memory[VAR_TMP_X] = rotated_point->x;
	script_memory[VAR_TMP_Y] = rotated_point->y;
}

void rotateFlipper(flipper_t *flipper, int8_t angle) BANKED {
	int16_t cosine = COS(angle);
	int16_t sine = SIN(angle);
	point16_t rotated_point;
	//rotatePoint(flipper->top_line.start_x, flipper->top_line.start_y, flipper->start_circle.center_x, flipper->start_circle.center_y, cosine, sine, &rotated_point);
	//flipper->top_line.start_x = rotated_point.x;
	//flipper->top_line.start_y = rotated_point.y;
	//if (angle < 0){
	//	flipper->top_line.start_x++;
	//	flipper->top_line.start_y++;
	//}
	rotatePoint(flipper->top_line.end_x, flipper->top_line.end_y, flipper->start_circle.center_x, flipper->start_circle.center_y, cosine, sine, &rotated_point);
	flipper->top_line.end_x = rotated_point.x;
	flipper->top_line.end_y = rotated_point.y;
	if (angle < 0){
		flipper->top_line.end_x++;
		flipper->top_line.end_y++;
	}
	script_memory[VAR_PIXELX] = flipper->top_line.end_x;
	script_memory[VAR_PIXELY] = flipper->top_line.end_y;
	
}

void actor_behavior_update(void) BANKED {
	flipper_t * flipper;
	actor_t * other_actor;
	for (UBYTE i = 0; i < MAX_ACTORS; i++){
		actor_t * actor = (actors + i);
		if (!actor->active){
			continue;
		}
		switch(actor_behavior_ids[i]){	
			case 0:
			break;
			case 1: //Debug left flipper start
				flipper = (flippers + 0);
				other_actor = (actors + flipper->actor_idx);
				actor->pos.x = PX_TO_SUBPX(flipper->top_line.start_x) + other_actor->pos.x;
				actor->pos.y = PX_TO_SUBPX(flipper->top_line.start_y) + other_actor->pos.y;
			break;
			case 2: //Debug left flipper end
				flipper = (flippers + 0);
				other_actor = (actors + flipper->actor_idx);
				actor->pos.x = PX_TO_SUBPX(flipper->top_line.end_x) + other_actor->pos.x;
				actor->pos.y = PX_TO_SUBPX(flipper->top_line.end_y) + other_actor->pos.y;
			break;
			case 3: //Debug right flipper start
				flipper = (flippers + 1);
				other_actor = (actors + flipper->actor_idx);
				actor->pos.x = PX_TO_SUBPX(flipper->top_line.start_x) + other_actor->pos.x;
				actor->pos.y = PX_TO_SUBPX(flipper->top_line.start_y) + other_actor->pos.y;
			break;
			case 4: //Debug right flipper end
				flipper = (flippers + 1);
				other_actor = (actors + flipper->actor_idx);
				actor->pos.x = PX_TO_SUBPX(flipper->top_line.end_x) + other_actor->pos.x;
				actor->pos.y = PX_TO_SUBPX(flipper->top_line.end_y) + other_actor->pos.y;
			break;
		}
	}
}

void pinball_init(void) BANKED {
	camera_offset_x = 0;
    camera_offset_y = 0;
    camera_deadzone_x = 0;
    camera_deadzone_y = 0;
	camera_settings = 0;
    game_time = 0;
    ball_grav = 1;
	ball_max_vel = 64;
	memset(balls, 0, sizeof(balls));
	memset(flippers, 0, sizeof(flippers));
	memset(bumper_circles, 0, sizeof(bumper_circles));
	memset(bumper_lines, 0, sizeof(bumper_lines));
	memset(actor_behavior_ids, 0, sizeof(actor_behavior_ids));
	balls_len = 0;
	flippers_len = 0;
	bumper_circles_len = 0;
	bumper_lines_len = 0;
}

void pinball_update(void) BANKED {
	actor_behavior_update();
	aggregate_x = 0;
	aggregate_y = 0;
	ball_nbr = 0;	
	bumper_circle_t * bumper_circle;
	bumper_line_t * bumper_line;
	int16_t ball_pos_x, ball_pos_y, ball_radius, ball_dot;
	uint8_t ball_collision_value, ball_collision_direction;
	int8_t ball_normal_x, ball_normal_y;
	point16_t closest_point;
	for (UBYTE ball_idx = 0; ball_idx < balls_len; ball_idx++){	
		ball_t * ball = (balls + ball_idx);
		actor_t * ball_actor = (actors + ball->actor_idx);
		if (!ball_actor->active){
			continue;
		}
		ball_radius = PX_TO_SUBPX((WORD)ball->circle.radius);	
		WORD clamped_vel_x = CLAMP(ball->vel_x, -ball_max_vel, ball_max_vel);
		WORD clamped_vel_y = CLAMP(ball->vel_y, -ball_max_vel, ball_max_vel);		
		ball_pos_x = (ball_actor->pos.x + PX_TO_SUBPX((WORD)ball->circle.center_x)) + clamped_vel_x;
		ball_pos_y = (ball_actor->pos.y + PX_TO_SUBPX((WORD)ball->circle.center_y)) + clamped_vel_y;
		
		if (ball->vel_y < ball_max_vel) {
			ball->vel_y += ball_grav;
		}
		
		if (ball_pos_x - ball_radius <= 0)
		{
			ball_pos_x = ball_radius + 1;
			ball->vel_x = -(ball->vel_x >> 2);
		}
		else if (ball_pos_x + ball_radius >= PX_TO_SUBPX(image_width))
		{
			ball_pos_x = PX_TO_SUBPX(image_width) - ball_radius - 1;
			ball->vel_x = -(ball->vel_x >> 2);
		}
		if (ball_pos_y - ball_radius <= 0)
		{
			ball_pos_y = ball_radius + 1;
			ball->vel_y = -(ball->vel_y >> 2);
		}
		else if (ball_pos_y + ball_radius >= PX_TO_SUBPX(image_height))
		{
			ball_pos_y = PX_TO_SUBPX(image_height) - ball_radius - 1;
			ball->vel_y = -(ball->vel_y >> 1);
		}
					
		ball_collision_value = get_collision(ball_pos_x, ball_pos_y);
		if (ball_collision_value)
		{	
			switch(ball_collision_value & 7){
				case 0://force field (slope)
				break;
				case 1:
				case 2://bouncy wall
				ball_collision_direction = ((((ball_collision_value & 0xF8) - 8) << 1) - 64) + ((rand() & 7) - 3);
				ball_normal_x = COS(ball_collision_direction);
				ball_normal_y = SIN(ball_collision_direction);
				ball->vel_x = ball_normal_x >> 1;
				ball->vel_y = ball_normal_y >> 1;
				ball_pos_x += SUBPX_TO_PX(ball_normal_x);
				ball_pos_y += SUBPX_TO_PX(ball_normal_y);
				break;
				case 3://normal wall
				ball_collision_direction = ((((ball_collision_value & 0xF8) - 8) << 1) - 64);
				ball_normal_x = COS(ball_collision_direction);
				ball_normal_y = SIN(ball_collision_direction);
				script_memory[VAR_NORMALX] = ball_normal_x;
				script_memory[VAR_NORMALY] = ball_normal_y;
				ball_dot = DOT(clamped_vel_x, ball_normal_x, clamped_vel_y, ball_normal_y);
				ball_dot = ((ball_dot << 1) - (ball_dot >> 1)) >> 6;
				ball->vel_x -= ((ball_dot * ball_normal_x) >> 8);
				ball->vel_y -= ((ball_dot * ball_normal_y) >> 8);
				script_memory[VAR_BALLVELX] = ball->vel_x;
				script_memory[VAR_BALLVELY] = ball->vel_y;
				ball_pos_x += (ball_normal_x) >> 4;
				ball_pos_y += (ball_normal_y) >> 4;
				script_memory[VAR_BALLX] = SUBPX_TO_PX(ball_pos_x);
				script_memory[VAR_BALLY] = SUBPX_TO_PX(ball_pos_y);
				script_memory[VAR_TILEX] = SUBPX_TO_TILE(ball_pos_x);
				script_memory[VAR_TILEY] = SUBPX_TO_TILE(ball_pos_y);
				break;
			}					
		}
		
		for (UBYTE flipper_idx = 0; flipper_idx < flippers_len; flipper_idx++){	
			flipper_t * flipper = (flippers + flipper_idx);
			actor_t * other_actor = (actors + flipper->actor_idx);
			if (!other_actor->active){
				continue;
			}	
			
			if (!ball_idx){
				flipper->rot_speed = 0;
				if (flipper->input && (INPUT_ANY & flipper->input)){					
					if (other_actor->frame < other_actor->frame_end - 1){
						other_actor->frame++;
						//other_actor->frame = other_actor->frame_end - 1;
						flipper->rot_speed = 1;
						//flipper->top_line.end_y -= 3;
						
						rotateFlipper(flipper, (flipper_idx ? 8: -8));
					}
				} else {
					if (other_actor->frame > other_actor->frame_start){
						other_actor->frame--;
						//other_actor->frame = other_actor->frame_start;
						//flipper->top_line.end_y += 3;

						rotateFlipper(flipper, (flipper_idx ? -8: 8));
					}
				}
			}			
			
			if (lineCircle((SUBPX_TO_PX(other_actor->pos.x) + flipper->top_line.start_x), 
						(SUBPX_TO_PX(other_actor->pos.y) + flipper->top_line.start_y),
						(SUBPX_TO_PX(other_actor->pos.x) + flipper->top_line.end_x),
						(SUBPX_TO_PX(other_actor->pos.y) + flipper->top_line.end_y),
						SUBPX_TO_PX(ball_pos_x), SUBPX_TO_PX(ball_pos_y), ball->circle.radius, closest_point)){
														
				
				ball_collision_direction = (atan2((-((WORD)(flipper->top_line.end_x) - (WORD)(flipper->top_line.start_x))), ((WORD)(flipper->top_line.end_y) - (WORD)(flipper->top_line.start_y))) - 64);// + ((rand() & 7) - 3);
				if (ball_collision_direction < 128){
					ball_collision_direction += 128;
				}
				script_memory[VAR_DIRECTION] = ball_collision_direction;
				ball_normal_x = COS(ball_collision_direction);
				ball_normal_y = SIN(ball_collision_direction);
				ball_dot = DOT(clamped_vel_x, ball_normal_x, clamped_vel_y, ball_normal_y);
				ball_dot = ((ball_dot << 1) - (ball_dot >> 1)) >> 6;	
				
				if (flipper->rot_speed){
					//ball->vel_x = ball_normal_x >> 1;
					//ball->vel_y = ball_normal_y >> 1;
					ball->vel_x -= (((ball_dot * ball_normal_x) >> 8) - ball_normal_x >> 1);
					ball->vel_y -= (((ball_dot * ball_normal_y) >> 8) - ball_normal_y >> 1) + 16;
					ball_pos_x += (ball_normal_x >> 4);
					ball_pos_y += (ball_normal_y >> 4) - 48;
				} else {
					ball->vel_x -= ((ball_dot * ball_normal_x) >> 8);
					ball->vel_y -= ((ball_dot * ball_normal_y) >> 8);
					ball_pos_x += (ball_normal_x >> 4);
					ball_pos_y += (ball_normal_y >> 4);
				}				
				script_memory[VAR_NORMALX] = ball_normal_x;
				script_memory[VAR_NORMALY] = ball_normal_y;
			}		
			
		}
		
		
		ball_actor->pos.y = ball_pos_y - PX_TO_SUBPX((WORD)ball->circle.center_y);
		ball_actor->pos.x = ball_pos_x - PX_TO_SUBPX((WORD)ball->circle.center_x);
		ball_nbr++;
		aggregate_x += ball_actor->pos.x;
		aggregate_y += ball_actor->pos.y;
	}
	//camera update
	UWORD a_x = (aggregate_x / ball_nbr) + 128;
	// Horizontal lock
	if (camera_x < a_x - PX_TO_SUBPX(camera_deadzone_x) - PX_TO_SUBPX(camera_offset_x)) { 
		camera_x = a_x - PX_TO_SUBPX(camera_deadzone_x) - PX_TO_SUBPX(camera_offset_x);
	} else if (camera_x > a_x + PX_TO_SUBPX(camera_deadzone_x) - PX_TO_SUBPX(camera_offset_x)) { 
		camera_x = a_x + PX_TO_SUBPX(camera_deadzone_x) - PX_TO_SUBPX(camera_offset_x);
	}
	
	UWORD a_y = (aggregate_y / ball_nbr) + 128;
	// Vertical lock
	if (camera_y < a_y - PX_TO_SUBPX(camera_deadzone_y) - PX_TO_SUBPX(camera_offset_y)) { 
		camera_y = a_y - PX_TO_SUBPX(camera_deadzone_y) - PX_TO_SUBPX(camera_offset_y);
	} else if (camera_y > a_y + PX_TO_SUBPX(camera_deadzone_y) - PX_TO_SUBPX(camera_offset_y)) { 
		camera_y = a_y + PX_TO_SUBPX(camera_deadzone_y) - PX_TO_SUBPX(camera_offset_y);
	}
	
	trigger_activate_at_intersection(&PLAYER.bounds, &PLAYER.pos, 0);
	
}

void vm_load_collision_masks(SCRIPT_CTX * THIS) OLDCALL BANKED {	
	uint8_t scene_bank = *(uint8_t *) VM_REF_TO_PTR(FN_ARG0);
	const scene_t * scene_ptr = *(scene_t **) VM_REF_TO_PTR(FN_ARG1);	
	scene_t scn;    MemcpyBanked(&scn, scene_ptr, sizeof(scn), scene_bank);
    MemcpyBanked(&collision_bkg, scn.background.ptr, sizeof(collision_bkg), scn.background.bank); 
    collision_data = scn.collisions;	
}

void vm_load_pinball_balls(SCRIPT_CTX * THIS) OLDCALL BANKED {
	uint8_t balls_bank = *(uint8_t *) VM_REF_TO_PTR(FN_ARG0);
	const ball_t * balls_ptr = *(ball_t **) VM_REF_TO_PTR(FN_ARG1);
	balls_len = *(uint8_t *) VM_REF_TO_PTR(FN_ARG2);
	MemcpyBanked(balls, balls_ptr, sizeof(ball_t) * (balls_len), balls_bank);
}

void vm_load_pinball_flippers(SCRIPT_CTX * THIS) OLDCALL BANKED {
	uint8_t flippers_bank = *(uint8_t *) VM_REF_TO_PTR(FN_ARG0);
	const flipper_t * flippers_ptr = *(flipper_t **) VM_REF_TO_PTR(FN_ARG1);
	flippers_len = *(uint8_t *) VM_REF_TO_PTR(FN_ARG2);
	MemcpyBanked(flippers, flippers_ptr, sizeof(flipper_t) * (flippers_len), flippers_bank);
}

void vm_load_pinball_bumper_circles(SCRIPT_CTX * THIS) OLDCALL BANKED {
	uint8_t bumper_circles_bank = *(uint8_t *) VM_REF_TO_PTR(FN_ARG0);
	const bumper_circle_t * bumper_circles_ptr = *(bumper_circle_t **) VM_REF_TO_PTR(FN_ARG1);
	bumper_circles_len = *(uint8_t *) VM_REF_TO_PTR(FN_ARG2);
	MemcpyBanked(bumper_circles, bumper_circles_ptr, sizeof(bumper_circle_t) * (bumper_circles_len), bumper_circles_bank);
}

void vm_load_pinball_bumper_lines(SCRIPT_CTX * THIS) OLDCALL BANKED {
	uint8_t bumper_lines_bank = *(uint8_t *) VM_REF_TO_PTR(FN_ARG0);
	const bumper_line_t * bumper_lines_ptr = *(bumper_line_t **) VM_REF_TO_PTR(FN_ARG1);
	bumper_lines_len = *(uint8_t *) VM_REF_TO_PTR(FN_ARG2);
	MemcpyBanked(bumper_lines, bumper_lines_ptr, sizeof(bumper_line_t) * (bumper_lines_len), bumper_lines_bank);
}

void vm_set_actor_behavior(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UBYTE actor_idx = *(uint8_t *)VM_REF_TO_PTR(FN_ARG0);
    UBYTE behavior_id = *(uint8_t *)VM_REF_TO_PTR(FN_ARG1);
    actor_behavior_ids[actor_idx] = behavior_id;
}