#version 410

struct ViewProjTransforms
{
	mat4 view_projection;
	mat4 view_projection_inverse;
};

layout (std140) uniform CameraViewProjTransforms
{
	ViewProjTransforms camera;
};

layout (std140) uniform LightViewProjTransforms
{
	ViewProjTransforms lights[100];
};

uniform int light_index;

uniform sampler2D depth_texture;
uniform sampler2D normal_texture;
uniform sampler2DShadow shadow_texture;

uniform vec2 inverse_screen_resolution;

uniform vec3 camera_position;

uniform vec3 light_color;
uniform vec3 light_position;
uniform vec3 light_direction;
uniform float light_intensity;
uniform float light_angle_falloff;

uniform vec2 shadowmap_texel_size;

layout (location = 0) out vec4 light_diffuse_contribution;
layout (location = 1) out vec4 light_specular_contribution;

void main()
{
	vec2 uv = gl_FragCoord.xy * inverse_screen_resolution;
	vec3 clip_pos = vec3(uv, texture(depth_texture, uv).x) * 2.0 - 1.0; //wasn't in the right range
	vec4 temp_pos = camera.view_projection_inverse * vec4(clip_pos, 1.0);
	vec3 world_pos = temp_pos.xyz / temp_pos.w; //perspective divide is a divition after all

	vec3 v2l = light_position - world_pos;
	vec3 L = normalize(v2l);
	vec3 N = normalize(texture(normal_texture, uv).xyz * 2.0 - 1.0);
	vec3 V = normalize(camera_position - world_pos);
	vec3 R = reflect(-L, N);
	float t = dot(-L, light_direction);
	float falloff = cos(light_angle_falloff);
	t = t > falloff ? (t - falloff) / (1.0 - falloff) : 0.0; //correct range for fade if within light influence

	vec4 light_screen_space = lights[light_index].view_projection * vec4(world_pos, 1.0);
	vec3 pels = light_screen_space.xyz / light_screen_space.w;

	pels = pels * 0.5 + 0.5;
	pels.z -= 0.0001;

	vec2 texel_size = vec2(1.0 / 1024);
	float shadow_depth = 0;
	int nbr = 10;
	pels.xy -= (nbr/2) * texel_size;
	vec2 pels_temp = pels.xy;

	for(int i = 0; i < nbr; i++){
		for(int j = 0; j < nbr; j++){
			shadow_depth += texture(shadow_texture, pels);
			pels.y += texel_size.y;
		}
		pels.y = pels_temp.y;
		pels.x += texel_size.x;
	}

	shadow_depth /= pow(nbr, 2);

	float shininess = 100;

	float intensity = light_intensity / pow(length(v2l),2.0);

	vec3 diffuse = shadow_depth * max(dot(N, L), 0.0) * light_color;
	vec3 specular = shadow_depth * max(pow(dot(V, R), shininess), 0.0) * light_color;

	light_diffuse_contribution  = vec4(intensity * t * diffuse, 1.0);
	light_specular_contribution = vec4(intensity * t * specular, 1.0);	
}
