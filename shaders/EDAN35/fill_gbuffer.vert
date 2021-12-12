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
	float height_scale = 1;
	float height_pow = 2.5;
	float height_offset = 0.055;
	vec3 _vertex = vertex;

	float height;
	float edge_correction = 0.01;
	if(texcoord.x < edge_correction || texcoord.y < edge_correction || texcoord.y > 1 - edge_correction) {
		height = 0;
	} else {
		height = texture2D(height_texture, texcoord.xy).r;
	}

	height = max(height - height_offset, 0.0);

	_vertex += pow(height, height_pow) * height_scale * normalize(normal);
	vs_out.normal   = normalize(normal);
	vs_out.texcoord = texcoord.xy;
	vs_out.tangent  = normalize(tangent);
	vs_out.binormal = normalize(binormal);

	gl_Position = camera.view_projection * vertex_model_to_world * vec4(_vertex, 1.0);
}
