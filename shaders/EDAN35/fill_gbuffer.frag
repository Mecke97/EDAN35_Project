#version 410

uniform float elapsed_time;
uniform bool has_diffuse_texture;
uniform bool has_specular_texture;
uniform bool has_normals_texture;
uniform bool has_opacity_texture;
uniform bool has_waves_texture;
uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normals_texture;
uniform sampler2D opacity_texture;
uniform sampler2D waves_texture1;
uniform sampler2D waves_texture2;
uniform sampler2D earth_normal_texture;
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

float random(float seed) {
	return fract(sin(seed) * 100000.0);
}

vec3 read_normal() {
	vec2 tex_scale1 = vec2(random(4.0), random(2.0)) * 50;
	vec2 tex_scale2 = vec2(random(5.0), random(1.0)) * 100;
	float norm_time = mod(elapsed_time, 100.0);
	vec2 norm_speed1 = vec2(1, 1) * 0.003;
	vec2 norm_speed2 = -norm_speed1;

	vec2 uv = fs_in.texcoord;

	vec2 ncoord0 = uv * tex_scale1 + norm_time * norm_speed1;
	vec2 ncoord1 = uv * tex_scale1 * 2 + norm_time * norm_speed1 * 4;
	vec2 ncoord2 = uv * tex_scale1 * 4 + norm_time * norm_speed1 * 8;

	vec2 ncoord3 = uv * tex_scale2 + norm_time * norm_speed2;
	vec2 ncoord4 = uv * tex_scale2 * 2 + norm_time * norm_speed2 * 4;
	vec2 ncoord5 = uv * tex_scale2 * 4 + norm_time * norm_speed2 * 8;

	vec4 bump = vec4(0.0);

	bump += texture(waves_texture1, ncoord0) * 2 - 1;
	bump += texture(waves_texture1, ncoord1) * 2 - 1;
	bump += texture(waves_texture1, ncoord2) * 2 - 1;

	bump += texture(waves_texture2, ncoord3) * 2 - 1;
	bump += texture(waves_texture2, ncoord4) * 2 - 1;
	bump += texture(waves_texture2, ncoord5) * 2 - 1;

	vec3 t = vec3(1.0, -fs_in.normal.x, 0.0);
	vec3 b = vec3(0.0, -fs_in.normal.z, 1.0);
	vec3 n = fs_in.normal;
	
	mat3 TBN = mat3(t, b, n);

	bump.xyz = normalize(TBN * vec3(bump));
	return bump.xyz;
}

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
		//geometry_normal.xyz = N * 0.5 + 0.5;
		geometry_normal.xyz = texture(earth_normal_texture, fs_in.texcoord).rgb;
		geometry_normal.xyz = normalize(geometry_normal.xyz * 2.0 - 1.0);
	}

	float wave_scale = 200.0;

	if(has_waves_texture && has_specular_texture && geometry_specular.r > 0.99) {
//		vec4 tex_norm = texture(waves_texture, fs_in.texcoord * wave_scale) * 2.0 - 1.0;
		geometry_normal.xyz = read_normal() * 0.5 + 0.5;
	}
}
