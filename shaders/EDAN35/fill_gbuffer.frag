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
uniform sampler2D foam_texture;
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

//	Classic Perlin 3D Noise 
//	by Stefan Gustavson
//
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
vec3 fade(vec3 t) {return t*t*t*(t*(t*6.0-15.0)+10.0);}

float cnoise(vec3 P){
  vec3 Pi0 = floor(P); // Integer part for indexing
  vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
  Pi0 = mod(Pi0, 289.0);
  Pi1 = mod(Pi1, 289.0);
  vec3 Pf0 = fract(P); // Fractional part for interpolation
  vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
  vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
  vec4 iy = vec4(Pi0.yy, Pi1.yy);
  vec4 iz0 = Pi0.zzzz;
  vec4 iz1 = Pi1.zzzz;

  vec4 ixy = permute(permute(ix) + iy);
  vec4 ixy0 = permute(ixy + iz0);
  vec4 ixy1 = permute(ixy + iz1);

  vec4 gx0 = ixy0 / 7.0;
  vec4 gy0 = fract(floor(gx0) / 7.0) - 0.5;
  gx0 = fract(gx0);
  vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
  vec4 sz0 = step(gz0, vec4(0.0));
  gx0 -= sz0 * (step(0.0, gx0) - 0.5);
  gy0 -= sz0 * (step(0.0, gy0) - 0.5);

  vec4 gx1 = ixy1 / 7.0;
  vec4 gy1 = fract(floor(gx1) / 7.0) - 0.5;
  gx1 = fract(gx1);
  vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
  vec4 sz1 = step(gz1, vec4(0.0));
  gx1 -= sz1 * (step(0.0, gx1) - 0.5);
  gy1 -= sz1 * (step(0.0, gy1) - 0.5);

  vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
  vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
  vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
  vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
  vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
  vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
  vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
  vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

  vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
  g000 *= norm0.x;
  g010 *= norm0.y;
  g100 *= norm0.z;
  g110 *= norm0.w;
  vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
  g001 *= norm1.x;
  g011 *= norm1.y;
  g101 *= norm1.z;
  g111 *= norm1.w;

  float n000 = dot(g000, Pf0);
  float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
  float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
  float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
  float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
  float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
  float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
  float n111 = dot(g111, Pf1);

  vec3 fade_xyz = fade(Pf0);
  vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
  vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
  float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x); 
  return 2.2 * n_xyz;
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
		geometry_normal.xyz = N * 0.5 + 0.5;
	}

	vec3 foam_color = vec3(1.0);

	if(has_waves_texture && geometry_specular.r > 0.99) {
		geometry_normal.xyz = read_normal() * 0.5 + 0.5;

		float foam_period = 30;
		float foam_freq = 0.09;
		float foam_sharpness = 10;
		float foam_contr = 0.0;

//		vec2 texel_size = 1.0 / vec2(10800.0, 5400.0);
//		float N = 3;
//
//		vec2 offset =  fs_in.texcoord - N * texel_size * 0.5;
//		for(float i = 0; i < N-0.001; i++) {
//			for(float j = 0; j < N-0.001; j++) {
//				vec2 uv = offset + vec2(texel_size.x * i, texel_size.y * j);
//				vec4 foam = texture(foam_texture, uv);
//				float foam_level = 1.0 - foam.b - foam.r;
//				float srf = cnoise(vec3(uv * 100, elapsed_time *0.1))*0.5 + 0.5;
//				float foam_sine = (sin((srf + foam_level + elapsed_time * foam_freq) * foam_period) + 1.0) * 0.5;
//				float foam_strength = foam_sine * pow(foam_level, foam_sharpness);
//				float rf = cnoise(vec3(uv * 300, elapsed_time *0.5))*0.5 + 0.5;
//
//				foam_contr += foam_strength * pow(rf,3);
//			}
//		}
//		foam_contr /= N*N;
//
		vec4 foam = texture(foam_texture, fs_in.texcoord);
		float foam_level = 1.0 - foam.b - foam.r;
		float srf = cnoise(vec3(fs_in.texcoord * 100, elapsed_time *0.1))*0.5 + 0.5;
		float foam_sine = (sin((srf + foam_level + elapsed_time * foam_freq) * foam_period) + 1.0) * 0.5;
		float foam_strength = foam_sine * pow(foam_level, foam_sharpness);
		float rf = cnoise(vec3(fs_in.texcoord * 300, elapsed_time *0.5))*0.5 + 0.5;

		foam_contr = foam_strength * pow(rf,3);

		geometry_diffuse.xyz += foam_color * foam_contr;
	}
}
