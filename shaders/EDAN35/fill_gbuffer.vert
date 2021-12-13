#version 410

uniform sampler2D height_texture;

struct ViewProjTransforms
{
	mat4 view_projection;
	mat4 view_projection_inverse;
};

layout (std140) uniform CameraViewProjTransforms
{
	ViewProjTransforms camera;
};

uniform mat4 vertex_model_to_world;

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;

out VS_OUT {
	vec3 normal;
	vec2 texcoord;
	vec3 tangent;
	vec3 binormal;
} vs_out;


void main() {
	float height_scale = 0.5;
	float height_pow = 0.2;
	float height_offset = 0;
	vec3 _vertex = vertex;

	float height;
	float edge_correction = 0.003;
	if(texcoord.x < edge_correction || texcoord.y < edge_correction || texcoord.y > 1 - edge_correction || texcoord.x > 1 - edge_correction) {
		height = 0.05;
	} else {
		float h_sum = 0;

		vec2 texel_size = 1.0 / vec2(10800.0, 5400.0);
		float N = 9;

		vec2 offset =  vec2(texcoord) - N * texel_size * 0.5;
		for(float i = 0; i < N-0.001; i++) {
			for(float j = 0; j < N-0.001; j++) {
				vec2 uv = offset + vec2(texel_size.x * i, texel_size.y * j);
				h_sum += texture(height_texture, uv).r;
			}
		}
		h_sum /= N*N;

		height = h_sum;
	}

	height = max(height - height_offset, 0.0);

	_vertex += pow(height, height_pow) * height_scale * normalize(normal);
	vs_out.normal   = normalize(normal);
	vs_out.texcoord = texcoord.xy;
	vs_out.tangent  = normalize(tangent);
	vs_out.binormal = normalize(binormal);

	gl_Position = camera.view_projection * vertex_model_to_world * vec4(_vertex, 1.0);
}
