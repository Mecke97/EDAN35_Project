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

void sample_dir(in vec4 curr, in vec2 s_dir, in vec2 uv, inout float dist) {
	vec2 rel_step = s_dir * texel_size;
	float step_size = length(rel_step);
	vec4 n = texture(flood_texture, uv + rel_step);

	if(n.r > 0.99) {
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
		if(curr.r < 0.99) {
			flood_result = vec4(1.0, 0.0, 0.0, 1.0);
		} else {
			flood_result = vec4(0.0, 0.0, 1.0, 1.0);
		}
	} else {
		curr = texture(flood_texture, uv);
		if(curr.r < 0.99) {
			vec2 s_r = vec2(1, 0);
			vec2 s_l = -s_r;
			vec2 s_u = vec2(0, 1);
			vec2 s_d = -s_u;


			float dist = 10000;
			sample_dir(curr, s_r, uv, dist);
			sample_dir(curr, s_l, uv, dist);
			sample_dir(curr, s_u, uv, dist);
			sample_dir(curr, s_d, uv, dist);

			flood_result = vec4(0.0, 0.0, dist, 1.0);
		} else {
			flood_result = curr;
		}
	}
}
