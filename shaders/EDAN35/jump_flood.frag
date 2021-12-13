#version 410

uniform sampler2D flood_texture;
uniform sampler2D specular_texture;
uniform bool is_init;
uniform vec2 texel_size;

in VS_OUT {
	vec2 texcoord;
} fs_in;

layout (location = 0) out vec4 flood_result;


#define STEP_SIZE 0.01
#define SAMPLES 32.0
#define DELTA_ANGLE 6.28318530718 / SAMPLES

void sample_dir(in vec2 uv, inout float min_dist, out bool next_to_land) {
	vec4 s = texture(flood_texture, uv);
	next_to_land = false;
	if(s.b < 0.9999 && s.r < 0.0001) {
		min_dist = min(min_dist, max(s.b-STEP_SIZE, STEP_SIZE));
	} else if(s.r > 0.9999) {
		min_dist = 1.0-STEP_SIZE;
		next_to_land = true;
	}
}

void sample_dir2(in vec4 curr, in vec2 s_dir, in vec2 uv, inout float dist) {
	vec2 rel_step = s_dir * texel_size;
	float step_size = length(rel_step);
	vec4 n = texture(flood_texture, uv + rel_step);

	if(n.r > 0.9999) {
		dist = 0;
	} else {
		dist = min(n.b + STEP_SIZE, min(curr.b, dist));
	}
}

void main()
{
	vec2 uv = fs_in.texcoord;
	vec4 curr;

	if(is_init) {
		curr = texture(specular_texture, uv);
		if(curr.r < 0.9999) {
			flood_result = vec4(1.0, 0.0, 0.0, 1.0);
		} else {
			flood_result = vec4(0.0, 0.0, 1.0, 1.0);
		}
	} else {
		curr = texture(flood_texture, uv);
//		if(curr.b > 0.9999) {
////			vec2 s_r = vec2(1, 0);
////			vec2 s_l = -s_r;
////			vec2 s_u = vec2(0, 1);
////			vec2 s_d = -s_u;
////			vec2 s_ur = s_u + s_r;
////			vec2 s_dl = -s_ur;
////			vec2 s_ul = s_u + s_l;
////			vec2 s_dr = -s_ul;
//
//			float min_dist = 1000.0;
//			bool next_to_land;
//
////			sample_dir(curr, uv + s_r * texel_size, min_dist);
////			sample_dir(curr, uv + s_l * texel_size, min_dist);
////			sample_dir(curr, uv + s_u * texel_size, min_dist);
////			sample_dir(curr, uv + s_d * texel_size, min_dist);
////			sample_dir(curr, uv + s_ur * texel_size, min_dist);
////			sample_dir(curr, uv + s_dl * texel_size, min_dist);
////			sample_dir(curr, uv + s_ul * texel_size, min_dist);
////			sample_dir(curr, uv + s_dr * texel_size, min_dist);
//
////			for(float i = 0; i < SAMPLES - 0.001; i++){
////				vec2 ds = vec2(cos(DELTA_ANGLE * i), sin(DELTA_ANGLE * i)) * texel_size;
////				vec2 s_dir = uv + ds;
////				sample_dir(s_dir, min_dist, next_to_land);
////
////				if(next_to_land)
////					break;
////			}
//		
//
////			flood_result = vec4(min_dist);
//			flood_result = vec4(0.0, 0.0, min_dist, 1.0);
//		} else {
//			flood_result = curr;
//		}
		if(curr.r < 0.9999) {
			vec2 s_r = vec2(1, 0);
			vec2 s_l = -s_r;
			vec2 s_u = vec2(0, 1);
			vec2 s_d = -s_u;


			float dist = 10000;
			sample_dir2(curr, s_r, uv, dist);
			sample_dir2(curr, s_l, uv, dist);
			sample_dir2(curr, s_u, uv, dist);
			sample_dir2(curr, s_d, uv, dist);

			flood_result = vec4(0.0, 0.0, dist, 1.0);
		} else {
			flood_result = curr;
		}
	}
}
