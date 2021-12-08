#version 410

uniform bool has_diffuse_texture;
uniform bool has_specular_texture;
uniform bool has_normals_texture;
uniform bool has_opacity_texture;
uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normals_texture;
uniform sampler2D opacity_texture;
uniform mat4 normal_model_to_world;

in VS_OUT {
	vec3 normal;
	vec2 texcoord;
	vec3 tangent;
	vec3 binormal;
} fs_in;

layout (location = 0) out vec4 geometry_diffuse;
layout (location = 1) out vec4 geometry_specular;
layout (location = 2) out vec4 geometry_normal;


void main()
{
	if (has_opacity_texture && texture(opacity_texture, fs_in.texcoord).r < 1.0)
		discard;

	// Diffuse color
	geometry_diffuse = vec4(0.0f);
	if (has_diffuse_texture)
		geometry_diffuse = texture(diffuse_texture, fs_in.texcoord);

	// Specular color
	geometry_specular = vec4(0.0f);
	if (has_specular_texture)
		geometry_specular = texture(specular_texture, fs_in.texcoord);

	// Worldspace normal
	geometry_normal = vec4(0.0);
	vec3 N = normalize(vec3(normal_model_to_world * vec4(fs_in.normal, 0.0)));
	vec3 B = normalize(vec3(normal_model_to_world * vec4(fs_in.binormal, 0.0)));
	vec3 T = normalize(vec3(normal_model_to_world * vec4(fs_in.tangent, 0.0)));
	if (has_normals_texture) {
		vec4 tex_norm = texture(normals_texture, fs_in.texcoord) * 2.0 - 1.0;
		geometry_normal.xyz = normalize(T * tex_norm.x + B * tex_norm.y + N * tex_norm.z) * 0.5 + 0.5;
	} else {
		geometry_normal.xyz = N * 0.5 + 0.5;
	}
}
