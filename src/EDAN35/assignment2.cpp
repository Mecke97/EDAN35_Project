// Do not use intrinsic functions, which allows using `constexpr` on GLM
// functions.
#define GLM_FORCE_PURE 1

#include "assignment2.hpp"

#include "config.hpp"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/helpers.hpp"
#include "core/node.hpp"
#include "core/opengl.hpp"
#include "core/ShaderProgramManager.hpp"
#include "EDAF80/parametric_shapes.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tinyfiledialogs.h>

#include <array>
#include <clocale>
#include <cstdlib>
#include <stdexcept>

namespace constant
{
	constexpr uint32_t shadowmap_res_x = 1024 * 2;
	constexpr uint32_t shadowmap_res_y = 1024 * 2;

	constexpr uint32_t earth_res_x = 10800;
	constexpr uint32_t earth_res_y = 5400;

	constexpr float  scale_lengths       = 1.0f; // The scene is expressed in centimetres rather than metres, hence the x100.

	constexpr size_t lights_nb           = 100;
	//constexpr float  light_intensity     = 72.0f * (scale_lengths * scale_lengths);
	//constexpr float  light_angle_falloff = glm::radians(37.0f);
}

namespace
{
	template <class E> constexpr auto toU(E const& e)
	{
		return static_cast<std::underlying_type_t<E>>(e);
	}

	enum class Texture : uint32_t {
		DepthBuffer = 0u,
		ShadowMap,
		GBufferDiffuse,
		GBufferSpecular,
		GBufferWorldSpaceNormal,
		LightDiffuseContribution,
		LightSpecularContribution,
		Result,
		Flood,
		Count
	};
	using Textures = std::array<GLuint, toU(Texture::Count)>;
	Textures createTextures(GLsizei framebuffer_width, GLsizei framebuffer_height);

	enum class Sampler : uint32_t {
		Nearest = 0u,
		Linear,
		Mipmaps,
		Shadow,
		Count
	};
	using Samplers = std::array<GLuint, toU(Sampler::Count)>;
	Samplers createSamplers();

	enum class FBO : uint32_t {
		GBuffer = 0u,
		ShadowMap,
		JumpFlood,
		LightAccumulation,
		Resolve,
		FinalWithDepth,
		Count
	};
	using FBOs = std::array<GLuint, toU(FBO::Count)>;
	FBOs createFramebufferObjects(Textures const& textures);

	enum class ElapsedTimeQuery : uint32_t {
		GbufferGeneration = 0u,
		ShadowMap0Generation,
		Light0Accumulation = ShadowMap0Generation + static_cast<uint32_t>(constant::lights_nb),
		Resolve = Light0Accumulation + static_cast<uint32_t>(constant::lights_nb),
		ConeWireframe,
		GUI,
		CopyToFramebuffer,
		Count
	};
	using ElapsedTimeQueries = std::array<GLuint, toU(ElapsedTimeQuery::Count)>;
	ElapsedTimeQueries createElapsedTimeQueries();

	enum class UBO : uint32_t {
		CameraViewProjTransforms = 0u,
		LightViewProjTransforms,
		Count
	};
	using UBOs = std::array<GLuint, toU(UBO::Count)>;
	UBOs createUniformBufferObjects();

	struct ViewProjTransforms
	{
		glm::mat4 view_projection = glm::mat4(1.0f);
		glm::mat4 view_projection_inverse = glm::mat4(1.0f);
	};

	struct ConeLight
	{
		TRSTransformf transform;
		ViewProjTransforms proj_transforms;
		glm::vec3 color;
		float intensity;
		float angle_falloff;
	};

	struct GeometryTextureData
	{
		GLuint diffuse_texture_id{ 0u };
		GLuint specular_texture_id{ 0u };
		GLuint normals_texture_id{ 0u };
		GLuint opacity_texture_id{ 0u };
		GLuint height_texture_id{ 0u };
		GLuint waves_texture1_id{ 0u };
		GLuint waves_texture2_id{ 0u };
		GLuint earth_normal_texture_id{ 0u };

	};

	struct GBufferShaderLocations
	{
		GLuint ubo_CameraViewProjTransforms{ 0u };
		GLuint vertex_model_to_world{ 0u };
		GLuint normal_model_to_world{ 0u };
		GLuint diffuse_texture{ 0u };
		GLuint specular_texture{ 0u };
		GLuint normals_texture{ 0u };
		GLuint opacity_texture{ 0u };
		GLuint has_diffuse_texture{ 0u };
		GLuint has_specular_texture{ 0u };
		GLuint has_normals_texture{ 0u };
		GLuint has_opacity_texture{ 0u };
		GLuint has_waves_texture{ 0u };
		GLuint height_texture{ 0u };
		GLuint waves_texture1{ 0u };
		GLuint waves_texture2{ 0u };
		GLuint earth_normal_texture{ 0u };
		GLuint foam_texture{ 0u };
		GLuint elapsed_time{ 0u };

	};
	void fillGBufferShaderLocations(GLuint gbuffer_shader, GBufferShaderLocations& locations);

	struct FillShadowmapShaderLocations
	{
		GLuint ubo_LightViewProjTransforms{ 0u };
		GLuint light_index{ 0u };
		GLuint vertex_model_to_world{ 0u };
		GLuint opacity_texture{ 0u };
		GLuint has_opacity_texture{ 0u };
	};
	void fillShadowmapShaderLocations(GLuint shadowmap_shader, FillShadowmapShaderLocations& locations);

	struct AccumulateLightsShaderLocations
	{
		GLuint ubo_CameraViewProjTransforms{ 0u };
		GLuint ubo_LightViewProjTransforms{ 0u };
		GLuint light_index{ 0u };
		GLuint vertex_model_to_world{ 0u };
		GLuint vertex_world_to_clip{ 0u };
		GLuint vertex_clip_to_world{ 0u };
		GLuint depth_texture{ 0u };
		GLuint normal_texture{ 0u };
		GLuint shadow_texture{ 0u };
		GLuint camera_position{ 0u };
		GLuint inverse_screen_resolution{ 0u };
		GLuint light_color{ 0u };
		GLuint light_position{ 0u };
		GLuint light_direction{ 0u };
		GLuint light_intensity{ 0u };
		GLuint light_angle_falloff{ 0u };
	};
	void fillAccumulateLightsShaderLocations(GLuint accumulate_lights_shader, AccumulateLightsShaderLocations& locations);

	bonobo::mesh_data loadCone();

	struct Airplane
	{
		float latitude = 0.0f;
		float longitude = -90.0f;
		glm::vec2 angular_velocity = glm::vec2(1.0f, 0.0f);
		float move_speed = 10.0f;
		float move_dir = 0.0f;
		float turn_speed = glm::radians(30.0f);
		Node* node;
	};


} // namespace

edan35::Assignment2::Assignment2(WindowManager& windowManager) :
	mCamera(0.5f * glm::half_pi<float>(),
	        static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
	        0.01f * constant::scale_lengths, 3000.0f * constant::scale_lengths),
	inputHandler(), mWindowManager(windowManager), window(nullptr)
{
	WindowManager::WindowDatum window_datum{ inputHandler, mCamera, config::resolution_x, config::resolution_y, 0, 0, 0, 0};

	window = mWindowManager.CreateGLFWWindow("EDAN35: Assignment 2", window_datum, config::msaa_rate);
	if (window == nullptr) {
		throw std::runtime_error("Failed to get a window: aborting!");
	}

	bonobo::init();
}

edan35::Assignment2::~Assignment2()
{
	bonobo::deinit();
}

void
edan35::Assignment2::run()
{
	float earth_radius = 200.0f;
	GLuint earth_diffuse_tex = bonobo::loadTexture2D(config::resources_path("project/earth_diffuse.jpg"));
	GLuint earth_specular_tex = bonobo::loadTexture2D(config::resources_path("project/earth_specular.jpg"));
	GLuint earth_height_tex = bonobo::loadTexture2D(config::resources_path("project/earth_height.jpg"));
	GLuint earth_normal_tex = bonobo::loadTexture2D(config::resources_path("project/canvas.png"));
	GLuint earth_wave_tex1 = bonobo::loadTexture2D(config::resources_path("project/waves1.jpg"));
	GLuint earth_wave_tex2 = bonobo::loadTexture2D(config::resources_path("project/waves2.jpg"));
	auto earth_geometry = parametric_shapes::createSphere(earth_radius * constant::scale_lengths, 10800/2, 5400/2);
	earth_geometry.bindings.insert(std::make_pair("diffuse_texture", earth_diffuse_tex));
	earth_geometry.bindings.insert(std::make_pair("specular_texture", earth_specular_tex));
	earth_geometry.bindings.insert(std::make_pair("height_texture", earth_height_tex));
	earth_geometry.bindings.insert(std::make_pair("normals_texture", earth_normal_tex));


	earth_geometry.bindings.insert(std::make_pair("waves_texture1", earth_wave_tex1));
	earth_geometry.bindings.insert(std::make_pair("waves_texture2", earth_wave_tex2));



	// Load the geometry of Sponza
	std::vector<bonobo::mesh_data> scene_geometry;
	scene_geometry.emplace_back(earth_geometry);

	std::vector<GeometryTextureData> sponza_geometry_texture_data;
	sponza_geometry_texture_data.reserve(scene_geometry.size());
	for (auto const& geometry : scene_geometry) {
		auto const diffuse_texture = geometry.bindings.find("diffuse_texture");
		auto const specular_texture = geometry.bindings.find("specular_texture");
		auto const normals_texture = geometry.bindings.find("normals_texture");
		auto const opacity_texture = geometry.bindings.find("opacity_texture");
		auto const height_texture = geometry.bindings.find("height_texture");
		auto const earth_normal_texture = geometry.bindings.find("normals_texture");


		auto const waves_texture1 = geometry.bindings.find("waves_texture1");
		auto const waves_texture2 = geometry.bindings.find("waves_texture2");



		GeometryTextureData data;
		if (diffuse_texture != geometry.bindings.end())
		{
			data.diffuse_texture_id = diffuse_texture->second;
		}
		if (specular_texture != geometry.bindings.end())
		{
			data.specular_texture_id = specular_texture->second;
		}
		if (normals_texture != geometry.bindings.end())
		{
			data.normals_texture_id = normals_texture->second;
		}
		if (opacity_texture != geometry.bindings.end())
		{
			data.opacity_texture_id = opacity_texture->second;
		}
		if (height_texture != geometry.bindings.end())
		{
			data.height_texture_id = height_texture->second;
		}
		if (waves_texture1 != geometry.bindings.end())
		{
			data.waves_texture1_id = waves_texture1->second;
			data.waves_texture2_id = waves_texture2->second;
		}
		if (earth_normal_texture != geometry.bindings.end())
		{
			data.earth_normal_texture_id = earth_normal_texture->second;
		}
		sponza_geometry_texture_data.emplace_back(std::move(data));
	}

	auto const cone_geometry = loadCone();
	Node cone;
	cone.set_geometry(cone_geometry);

	//
	// Setup the camera
	//

	float camera_height = 30;
	mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 1.0f, earth_radius + camera_height) * constant::scale_lengths);
	mCamera.mMouseSensitivity = 0.003f;
	mCamera.mMovementSpeed = 3.0f * constant::scale_lengths; // 3 m/s => 10.8 km/h.

	int framebuffer_width, framebuffer_height;
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

	//
	// Setup OpenGL objects
	// Look further down in this file to see the implementation of those functions.
	//
	Textures const textures = createTextures(framebuffer_width, framebuffer_height);
	FBOs const fbos = createFramebufferObjects(textures);
	Samplers const samplers = createSamplers();
	ElapsedTimeQueries const elapsed_time_queries = createElapsedTimeQueries();
	UBOs const ubos = createUniformBufferObjects();

	//
	// Load all the shader programs used
	//
	ShaderProgramManager program_manager;
	GLuint fallback_shader = 0u;
	program_manager.CreateAndRegisterProgram("Fallback",
	                                         { { ShaderType::vertex, "common/fallback.vert" },
	                                           { ShaderType::fragment, "common/fallback.frag" } },
	                                         fallback_shader);
	if (fallback_shader == 0u) {
		LogError("Failed to load fallback shader");
		return;
	}

	GLuint fill_gbuffer_shader = 0u;
	program_manager.CreateAndRegisterProgram("Fill G-Buffer",
	                                         { { ShaderType::vertex, "EDAN35/fill_gbuffer.vert" },
	                                           { ShaderType::fragment, "EDAN35/fill_gbuffer.frag" } },
	                                         fill_gbuffer_shader);
	if (fill_gbuffer_shader == 0u) {
		LogError("Failed to load G-buffer filling shader");
		return;
	}
	GBufferShaderLocations fill_gbuffer_shader_locations;
	fillGBufferShaderLocations(fill_gbuffer_shader, fill_gbuffer_shader_locations);

	GLuint fill_shadowmap_shader = 0u;
	program_manager.CreateAndRegisterProgram("Fill shadow map",
	                                         { { ShaderType::vertex, "EDAN35/fill_shadowmap.vert" },
	                                           { ShaderType::fragment, "EDAN35/fill_shadowmap.frag" } },
	                                         fill_shadowmap_shader);
	if (fill_shadowmap_shader == 0u) {
		LogError("Failed to load shadowmap filling shader");
		return;
	}
	FillShadowmapShaderLocations fill_shadowmap_shader_locations;
	fillShadowmapShaderLocations(fill_shadowmap_shader, fill_shadowmap_shader_locations);

	GLuint accumulate_lights_shader = 0u;
	program_manager.CreateAndRegisterProgram("Accumulate light",
	                                         { { ShaderType::vertex, "EDAN35/accumulate_lights.vert" },
	                                           { ShaderType::fragment, "EDAN35/accumulate_lights.frag" } },
	                                         accumulate_lights_shader);
	if (accumulate_lights_shader == 0u) {
		LogError("Failed to load lights accumulating shader");
		return;
	}
	AccumulateLightsShaderLocations accumulate_light_shader_locations;
	fillAccumulateLightsShaderLocations(accumulate_lights_shader, accumulate_light_shader_locations);

	GLuint resolve_deferred_shader = 0u;
	program_manager.CreateAndRegisterProgram("Resolve deferred",
	                                         { { ShaderType::vertex, "EDAN35/resolve_deferred.vert" },
	                                           { ShaderType::fragment, "EDAN35/resolve_deferred.frag" } },
	                                         resolve_deferred_shader);
	if (resolve_deferred_shader == 0u) {
		LogError("Failed to load deferred resolution shader");
		return;
	}

	GLuint render_light_cones_shader = 0u;
	program_manager.CreateAndRegisterProgram("Render light cones",
	                                         { { ShaderType::vertex, "EDAN35/render_light_cones.vert" },
	                                           { ShaderType::fragment, "EDAN35/render_light_cones.frag" } },
	                                         render_light_cones_shader);
	if (render_light_cones_shader == 0u) {
		LogError("Failed to load light cones rendering shader");
		return;
	}

	GLuint jump_flood_shader = 0u;
	program_manager.CreateAndRegisterProgram("Jump flood",
											{ { ShaderType::vertex, "EDAN35/jump_flood.vert" },
											  { ShaderType::fragment, "EDAN35/jump_flood.frag" } },
											jump_flood_shader);
	if (jump_flood_shader == 0u)
	{
		LogError("Failed to load jump flood shader");
		return;
	}

	auto const set_uniforms = [](GLuint /*program*/){};

	ViewProjTransforms camera_view_proj_transforms;
	std::array<ViewProjTransforms, constant::lights_nb> light_view_proj_transforms;

	const GLuint debug_texture_id = bonobo::getDebugTextureID();

	auto const bind_texture_with_sampler = [](GLenum target, unsigned int slot, GLuint program, std::string const& name, GLuint texture, GLuint sampler){
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(target, texture);
		glUniform1i(glGetUniformLocation(program, name.c_str()), static_cast<GLint>(slot));
		glBindSampler(slot, sampler);
	};

	//
	// Setup lights properties
	//
	bool are_lights_paused = false;


	float const lightProjectionNearPlane = 0.01f * constant::scale_lengths;
	float const lightProjectionFarPlane = 150.0f * constant::scale_lengths;
	auto lightProjection = glm::perspective(0.5f * glm::pi<float>(),
	                                        static_cast<float>(constant::shadowmap_res_x) / static_cast<float>(constant::shadowmap_res_y),
	                                        lightProjectionNearPlane, lightProjectionFarPlane);

	std::vector<ConeLight> lights;

	ConeLight sun;
	sun.angle_falloff = glm::radians(85.0f);
	sun.color = glm::vec3(1.0f);
	sun.intensity = 11700.0f;

	float sun_height = 70.0f;

	sun.transform.SetTranslate(glm::vec3(0.0f, 0.0f, earth_radius + sun_height) * constant::scale_lengths);

	lights.push_back(sun);

	//int lights_nb = static_cast<int>(constant::lights_nb);

	TRSTransformf coneScaleTransform;
	coneScaleTransform.SetScale(glm::vec3(lightProjectionFarPlane * 0.8f));

	TRSTransformf lightOffsetTransform;
	lightOffsetTransform.SetTranslate(glm::vec3(0.0f, 0.0f, -0.4f) * constant::scale_lengths);


	glClearColor(0.0f, 0.2f, 0.3f, 1.0f);
	glClearDepthf(1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);


	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[toU(FBO::Resolve)]);


	//Setup nodes
	std::vector<Node*> nodes;

	auto plane_geometry = bonobo::loadObjects(config::resources_path("project/airplane/airplane.obj"));
	if (plane_geometry.empty())
	{
		LogError("Failed to load the airplane model");
		return;
	}

	auto plane_diffuse_tex = bonobo::loadTexture2D(config::resources_path("project/airplane/textures/diffuse.jpg"));
	auto plane_normals_tex = bonobo::loadTexture2D(config::resources_path("project/airplane/textures/normal.png"));

	for (auto md : plane_geometry)
	{
		md.bindings.insert(std::make_pair("diffuse_texture", plane_diffuse_tex));
		md.bindings.insert(std::make_pair("normals_texture", plane_normals_tex));
	}

	Node plane;
	plane.set_geometry(plane_geometry.at(0));

	auto plane_elevation = 35.0f;
	
	plane.get_transform().SetTranslate(glm::vec3(0, 0, earth_radius + plane_elevation) * constant::scale_lengths);
	plane.get_transform().RotateX(glm::radians(-90.0f));
	plane.get_transform().RotateY(glm::radians(180.0f));
	plane.get_transform().SetScale(0.05 * constant::scale_lengths);

	nodes.push_back(&plane);

	Node* c;
	for (int i = 1; i < plane_geometry.size(); i++)
	{
		c = new Node();
		c->set_geometry(plane_geometry[i]);

		nodes.push_back(c);
		plane.add_child(c);
	}

	std::vector<GeometryTextureData> nodes_texture_data;
	nodes_texture_data.reserve(nodes.size());
	for (auto const node : nodes)
	{
		auto geometry = node->geometry;

		geometry.bindings.insert(std::make_pair("diffuse_texture", plane_diffuse_tex));
		geometry.bindings.insert(std::make_pair("normals_texture", plane_normals_tex));

		auto const diffuse_texture = geometry.bindings.find("diffuse_texture");
		auto const specular_texture = geometry.bindings.find("specular_texture");
		auto const normals_texture = geometry.bindings.find("normals_texture");
		auto const opacity_texture = geometry.bindings.find("opacity_texture");

		GeometryTextureData data;
		if (diffuse_texture != geometry.bindings.end())
		{
			LogTrivia("Found diffuse");
			data.diffuse_texture_id = diffuse_texture->second;
		}
		if (specular_texture != geometry.bindings.end())
		{
			data.specular_texture_id = specular_texture->second;
		}
		if (normals_texture != geometry.bindings.end())
		{
			LogTrivia("Found normals");
			data.normals_texture_id = normals_texture->second;
		}
		if (opacity_texture != geometry.bindings.end())
		{
			data.opacity_texture_id = opacity_texture->second;
		}
		nodes_texture_data.emplace_back(std::move(data));
	}

	

	auto seconds_nb = 0.0f;
	std::array<GLuint64, toU(ElapsedTimeQuery::Count)> pass_elapsed_times;
	auto lastTime = std::chrono::high_resolution_clock::now();
	bool show_textures = true;
	bool show_cone_wireframe = false;

	bool show_logs = false;
	bool show_gui = true;
	bool shader_reload_failed = false;
	bool copy_elapsed_times = true;
	bool first_frame = true;
	bool show_basis = false;
	float basis_thickness_scale = 40.0f;
	float basis_length_scale = 400.0f;

	bool track_plane = true;

	Airplane airplane;
	airplane.node = &plane;
	airplane.move_speed = 10.0f;


	auto jump_flood_geometry = parametric_shapes::createQuad(1, 1);

	float tilt_amount = 0.7f;
	float catch_up = 1.6f;
	bool smoothed_camera = false;

	//Main loop
	while (!glfwWindowShouldClose(window)) {
		auto const nowTime = std::chrono::high_resolution_clock::now();
		auto const deltaTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(nowTime - lastTime);

		auto const deltaTimeS = std::chrono::duration<decltype(seconds_nb)>(deltaTimeUs).count();

		lastTime = nowTime;
		if (!are_lights_paused)
			seconds_nb += std::chrono::duration<decltype(seconds_nb)>(deltaTimeUs).count();

		auto& io = ImGui::GetIO();
		inputHandler.SetUICapture(io.WantCaptureMouse, io.WantCaptureKeyboard);

		glfwPollEvents();
		inputHandler.Advance();
		if (!track_plane)
		{
			mCamera.mWorld.LookTowards(mCamera.mWorld.GetFront(), glm::vec3(0.0f, 1.0, 0.0f));
			mCamera.Update(deltaTimeUs, inputHandler);
		}

		camera_view_proj_transforms.view_projection = mCamera.GetWorldToClipMatrix();
		camera_view_proj_transforms.view_projection_inverse = mCamera.GetClipToWorldMatrix();

		auto const view_projection = camera_view_proj_transforms.view_projection;

		if (inputHandler.GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
			shader_reload_failed = !program_manager.ReloadAllPrograms();
			if (shader_reload_failed)
			{
				tinyfd_notifyPopup("Shader Program Reload Error",
				                   "An error occurred while reloading shader programs; see the logs for details.\n"
				                   "Rendering is suspended until the issue is solved. Once fixed, just reload the shaders again.",
				                   "error");
			}
			else
			{
				fillGBufferShaderLocations(fill_gbuffer_shader, fill_gbuffer_shader_locations);
				fillShadowmapShaderLocations(fill_shadowmap_shader, fill_shadowmap_shader_locations);
				fillAccumulateLightsShaderLocations(accumulate_lights_shader, accumulate_light_shader_locations);
			}
		}
		if (inputHandler.GetKeycodeState(GLFW_KEY_F3) & JUST_RELEASED)
			show_logs = !show_logs;
		if (inputHandler.GetKeycodeState(GLFW_KEY_F2) & JUST_RELEASED)
			show_gui = !show_gui;

		mWindowManager.NewImGuiFrame();

		if (!first_frame && show_gui && copy_elapsed_times) {
			// Copy all timings back from the GPU to the CPU.
			for (GLuint i = 0; i < pass_elapsed_times.size(); ++i) {
				glGetQueryObjectui64v(elapsed_time_queries[i], GL_QUERY_RESULT, pass_elapsed_times.data() + i);
			}
		}


		for (size_t i = 0; i < lights.size(); ++i) {
			auto& lightTransform = lights[i].transform;

			auto const light_view_matrix = lightTransform.GetMatrixInverse();
			auto const light_world_matrix = glm::inverse(light_view_matrix);
			auto const light_world_to_clip_matrix = lightProjection * light_view_matrix;

			light_view_proj_transforms[i].view_projection = light_world_to_clip_matrix;
			light_view_proj_transforms[i].view_projection_inverse = glm::inverse(light_world_to_clip_matrix);
		}

		//
		//Game logic
		//

		float movementModifier = 1.0f;
		if (!inputHandler.IsKeyboardCapturedByUI() && track_plane)
		{
			movementModifier = (inputHandler.GetKeycodeState(GLFW_KEY_LEFT_SHIFT) & PRESSED) ? 2.5f : 1.0f;
			if ((inputHandler.GetKeycodeState(GLFW_KEY_A) & PRESSED)) airplane.move_dir -= airplane.turn_speed * deltaTimeS;
			if ((inputHandler.GetKeycodeState(GLFW_KEY_D) & PRESSED)) airplane.move_dir += airplane.turn_speed * deltaTimeS;
		}

		auto to_center = -glm::normalize(plane.get_transform().GetTranslation());
		auto up = glm::vec3(0, 1, 0);
		auto rot = glm::quat(glm::cos(0.5f * airplane.move_dir), to_center * glm::sin(0.5f * airplane.move_dir));

		bool up_aligned = glm::abs(glm::dot(to_center, up)) > 0.9999f;
		auto move_dir = glm::rotate(rot, glm::normalize(glm::cross(up_aligned ? glm::vec3(1, 0, 0) : up, to_center)));


		plane.get_transform().Translate(move_dir * airplane.move_speed * deltaTimeS * movementModifier);
		if (up_aligned) plane.get_transform().Translate(-move_dir);
		auto to_center_new = -glm::normalize(plane.get_transform().GetTranslation());
		auto correction_v = -to_center_new * (earth_radius + plane_elevation) - plane.get_transform().GetTranslation();
		plane.get_transform().Translate(correction_v);

		plane.get_transform().LookTowards(glm::normalize(-move_dir), glm::normalize(-to_center_new));

		

		if (track_plane)
		{
			auto plane_pos = plane.get_transform().GetTranslation();
			auto look_offset = glm::normalize(move_dir) * tilt_amount;
			auto up_offset = glm::normalize(-to_center_new) * tilt_amount;
			auto trans_offset = glm::normalize(-move_dir) * tilt_amount;

			auto desired_pos = plane_pos - to_center_new * camera_height + trans_offset * camera_height;
			glm::vec3 actual_pos;
			if (smoothed_camera)
			{
				auto current_pos = mCamera.mWorld.GetTranslation();
				actual_pos = (desired_pos - current_pos) * glm::min(catch_up * deltaTimeS, 1.0f) + current_pos;
			}
			else
			{
				actual_pos = desired_pos;
			}

			mCamera.mWorld.SetTranslate(actual_pos);
			mCamera.mWorld.LookTowards(glm::normalize(to_center_new + look_offset), glm::normalize(move_dir + up_offset));

			auto sun_offset = (plane.get_transform().GetRight() + plane.get_transform().GetBack()) * 30.0f * constant::scale_lengths;

			lights[0].transform.SetTranslate(plane.get_transform().GetTranslation() + sun_offset - to_center_new * 45.0f);
			lights[0].transform.LookTowards(glm::normalize(to_center_new), glm::normalize(move_dir));
		}


		//
		// Update per-frame changing UBOs.
		//
		glBindBuffer(GL_UNIFORM_BUFFER, ubos[toU(UBO::CameraViewProjTransforms)]);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(camera_view_proj_transforms), &camera_view_proj_transforms);
		glBindBuffer(GL_UNIFORM_BUFFER, ubos[toU(UBO::LightViewProjTransforms)]);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(light_view_proj_transforms), light_view_proj_transforms.data());
		glBindBuffer(GL_UNIFORM_BUFFER, 0u);

		if (!shader_reload_failed) {
			//
			// Pass 0: Update jump flood texture
			//

			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[toU(FBO::JumpFlood)]);
			glViewport(0, 0, constant::earth_res_x, constant::earth_res_y);

			glUseProgram(jump_flood_shader);

			glUniform1i(glGetUniformLocation(jump_flood_shader, "is_init"), first_frame ? 1 : 0);
			glUniform2f(glGetUniformLocation(jump_flood_shader, "texel_size"), 1.0f/ constant::earth_res_x, 1.0f/ constant::earth_res_y);

			auto const sampler = samplers[toU(Sampler::Nearest)];

			glUniform1i(glGetUniformLocation(jump_flood_shader, "flood_texture"), 0);
			glBindSampler(0u, sampler);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::Flood)]);

			glUniform1i(glGetUniformLocation(jump_flood_shader, "specular_texture"), 1);
			glBindSampler(1u, sampler);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, earth_specular_tex);

			glBindVertexArray(jump_flood_geometry.vao);
			if (jump_flood_geometry.ibo != 0u)
				glDrawElements(jump_flood_geometry.drawing_mode, jump_flood_geometry.indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
			else
				glDrawArrays(jump_flood_geometry.drawing_mode, 0, jump_flood_geometry.vertices_nb);

			glBindTexture(GL_TEXTURE_2D, 0);
			glBindVertexArray(0u);
			glUseProgram(0u);

			//
			// Pass 1: Render scene into the g-buffer
			//
			utils::opengl::debug::beginDebugGroup("Fill G-buffer");
			glBeginQuery(GL_TIME_ELAPSED, elapsed_time_queries[toU(ElapsedTimeQuery::GbufferGeneration)]);

			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[toU(FBO::GBuffer)]);
			glViewport(0, 0, framebuffer_width, framebuffer_height);
			glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
			// XXX: Is any other clearing needed?

			glUseProgram(fill_gbuffer_shader);
			glUniform1i(fill_gbuffer_shader_locations.diffuse_texture, 0);
			glUniform1i(fill_gbuffer_shader_locations.specular_texture, 1);
			glUniform1i(fill_gbuffer_shader_locations.normals_texture, 2);
			glUniform1i(fill_gbuffer_shader_locations.opacity_texture, 3);
			glUniform1i(fill_gbuffer_shader_locations.height_texture, 4);
			glUniform1i(fill_gbuffer_shader_locations.waves_texture1, 5);
			glUniform1i(fill_gbuffer_shader_locations.waves_texture2, 6);
			glUniform1i(fill_gbuffer_shader_locations.foam_texture, 7);
			glUniform1i(fill_gbuffer_shader_locations.earth_normal_texture, 8);


			glUniform1f(fill_gbuffer_shader_locations.elapsed_time, seconds_nb);

			for (std::size_t i = 0; i < scene_geometry.size(); ++i)
			{
				auto const& geometry = scene_geometry[i];
				auto const& texture_data = sponza_geometry_texture_data[i];

				utils::opengl::debug::beginDebugGroup(geometry.name);

				auto const vertex_model_to_world = glm::mat4(1.0f);
				auto const normal_model_to_world = glm::mat4(1.0f);

				glUniformMatrix4fv(fill_gbuffer_shader_locations.vertex_model_to_world, 1, GL_FALSE, glm::value_ptr(vertex_model_to_world));
				glUniformMatrix4fv(fill_gbuffer_shader_locations.normal_model_to_world, 1, GL_FALSE, glm::value_ptr(normal_model_to_world));

				auto const default_sampler = samplers[toU(Sampler::Nearest)];
				auto const mipmap_sampler = samplers[toU(Sampler::Mipmaps)];

				glUniform1i(fill_gbuffer_shader_locations.has_diffuse_texture, texture_data.diffuse_texture_id != 0u ? 1 : 0);
				glBindSampler(0u, default_sampler);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, texture_data.diffuse_texture_id != 0u ? texture_data.diffuse_texture_id : debug_texture_id);

				glUniform1i(fill_gbuffer_shader_locations.has_specular_texture, texture_data.specular_texture_id != 0u ? 1 : 0);
				glBindSampler(1u, texture_data.specular_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, texture_data.specular_texture_id != 0u ? texture_data.specular_texture_id : debug_texture_id);

				glUniform1i(fill_gbuffer_shader_locations.has_normals_texture, texture_data.normals_texture_id != 0u ? 1 : 0);
				glBindSampler(2u, texture_data.normals_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, texture_data.normals_texture_id != 0u ? texture_data.normals_texture_id : debug_texture_id);

				glUniform1i(fill_gbuffer_shader_locations.has_opacity_texture, texture_data.opacity_texture_id != 0u ? 1 : 0);
				glBindSampler(3u, texture_data.opacity_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, texture_data.opacity_texture_id != 0u ? texture_data.opacity_texture_id : debug_texture_id);

				glBindSampler(4u, texture_data.height_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, texture_data.height_texture_id != 0u ? texture_data.height_texture_id : debug_texture_id);

				bool has_wave = texture_data.waves_texture1_id != 0u && texture_data.waves_texture2_id != 0u;
				glUniform1i(fill_gbuffer_shader_locations.has_waves_texture, has_wave ? 1 : 0);
				glBindSampler(5u, has_wave ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, has_wave ? texture_data.waves_texture1_id : debug_texture_id);
				glBindSampler(6u, has_wave ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_2D, has_wave ? texture_data.waves_texture2_id : debug_texture_id);

				glBindSampler(7u, samplers[toU(Sampler::Linear)]);
				glActiveTexture(GL_TEXTURE7);
				glBindTexture(GL_TEXTURE_2D,  textures[toU(Texture::Flood)]);

				glBindSampler(8u, texture_data.earth_normal_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE8);
				glBindTexture(GL_TEXTURE_2D, texture_data.earth_normal_texture_id != 0u ? texture_data.earth_normal_texture_id : debug_texture_id);

				glBindVertexArray(geometry.vao);
				if (geometry.ibo != 0u)
					glDrawElements(geometry.drawing_mode, geometry.indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
				else
					glDrawArrays(geometry.drawing_mode, 0, geometry.vertices_nb);


				utils::opengl::debug::endDebugGroup();
			}

			for (std::size_t i = 0; i < nodes.size(); ++i)
			{
				Node node = *(nodes[i]);
				auto const& geometry = node.geometry;
				auto const& texture_data = nodes_texture_data[i];

				auto vertex_model_to_world = node.get_transform().GetMatrix();

				Node* parent = node.parent;
				while (parent)
				{
					vertex_model_to_world = parent->get_transform().GetMatrix() * vertex_model_to_world;
					parent = parent->parent;
				}

				auto normal_model_to_world = glm::transpose(glm::inverse(vertex_model_to_world));

				glUniformMatrix4fv(fill_gbuffer_shader_locations.vertex_model_to_world, 1, GL_FALSE, glm::value_ptr(vertex_model_to_world));
				glUniformMatrix4fv(fill_gbuffer_shader_locations.normal_model_to_world, 1, GL_FALSE, glm::value_ptr(normal_model_to_world));

				auto const default_sampler = samplers[toU(Sampler::Nearest)];
				auto const mipmap_sampler = samplers[toU(Sampler::Mipmaps)];

				glUniform1i(fill_gbuffer_shader_locations.has_diffuse_texture, texture_data.diffuse_texture_id != 0u ? 1 : 0);
				glBindSampler(0u, texture_data.diffuse_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, texture_data.diffuse_texture_id != 0u ? texture_data.diffuse_texture_id : debug_texture_id);

				glUniform1i(fill_gbuffer_shader_locations.has_specular_texture, texture_data.specular_texture_id != 0u ? 1 : 0);
				glBindSampler(1u, texture_data.specular_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, texture_data.specular_texture_id != 0u ? texture_data.specular_texture_id : debug_texture_id);

				glUniform1i(fill_gbuffer_shader_locations.has_normals_texture, texture_data.normals_texture_id != 0u ? 1 : 0);
				glBindSampler(2u, texture_data.normals_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, texture_data.normals_texture_id != 0u ? texture_data.normals_texture_id : debug_texture_id);

				glUniform1i(fill_gbuffer_shader_locations.has_opacity_texture, texture_data.opacity_texture_id != 0u ? 1 : 0);
				glBindSampler(3u, texture_data.opacity_texture_id != 0u ? mipmap_sampler : default_sampler);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, texture_data.opacity_texture_id != 0u ? texture_data.opacity_texture_id : debug_texture_id);

				glBindVertexArray(geometry.vao);
				if (geometry.ibo != 0u)
					glDrawElements(geometry.drawing_mode, geometry.indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
				else
					glDrawArrays(geometry.drawing_mode, 0, geometry.vertices_nb);

			}

			glBindTexture(GL_TEXTURE_2D, 0);
			glBindVertexArray(0u);
			glUseProgram(0u);

			glEndQuery(GL_TIME_ELAPSED);
			utils::opengl::debug::endDebugGroup();



			//
			// Pass 2: Generate shadowmaps and accumulate lights' contribution
			//
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[toU(FBO::LightAccumulation)]);
			glViewport(0, 0, framebuffer_width, framebuffer_height);
			glClear(GL_COLOR_BUFFER_BIT);
			// XXX: Is any clearing needed?
			for (size_t i = 0; i < lights.size(); ++i) {
				auto const& lightTransform = lights[i].transform;
				auto const light_view_matrix = lightTransform.GetMatrixInverse();
				auto const light_world_matrix = glm::inverse(light_view_matrix) * coneScaleTransform.GetMatrix();
				//auto const light_world_to_clip_matrix = lightProjection * light_view_matrix;

				//
				// Pass 2.1: Generate shadow map for light i
				//
				utils::opengl::debug::beginDebugGroup("Create shadow map " + std::to_string(i));
				glBeginQuery(GL_TIME_ELAPSED, elapsed_time_queries[toU(ElapsedTimeQuery::ShadowMap0Generation) + i]);

				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[toU(FBO::ShadowMap)]);
				glViewport(0, 0, constant::shadowmap_res_x, constant::shadowmap_res_y);
				glClear(GL_DEPTH_BUFFER_BIT);
				// XXX: Is any clearing needed?

				glUseProgram(fill_shadowmap_shader);
				glUniform1i(fill_shadowmap_shader_locations.light_index, static_cast<int>(i));
				glUniform1i(fill_shadowmap_shader_locations.opacity_texture, 0);
				for (std::size_t i = 0; i < scene_geometry.size(); ++i)
				{
					auto const& geometry = scene_geometry[i];
					auto const& texture_data = sponza_geometry_texture_data[i];

					utils::opengl::debug::beginDebugGroup(geometry.name);

					auto const vertex_model_to_world = glm::mat4(1.0f);
					glUniformMatrix4fv(fill_shadowmap_shader_locations.vertex_model_to_world, 1, GL_FALSE, glm::value_ptr(vertex_model_to_world));

					glUniform1i(fill_shadowmap_shader_locations.has_opacity_texture, texture_data.opacity_texture_id != 0u ? 1 : 0);
					glBindSampler(0u, texture_data.opacity_texture_id != 0u ? samplers[toU(Sampler::Mipmaps)] : samplers[toU(Sampler::Nearest)]);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, texture_data.opacity_texture_id != 0u ? texture_data.opacity_texture_id : debug_texture_id);

					glBindVertexArray(geometry.vao);
					if (geometry.ibo != 0u)
						glDrawElements(geometry.drawing_mode, geometry.indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
					else
						glDrawArrays(geometry.drawing_mode, 0, geometry.vertices_nb);


					utils::opengl::debug::endDebugGroup();
				}

				for (std::size_t i = 0; i < nodes.size(); ++i)
				{
					Node node = *(nodes[i]);
					auto const& geometry = node.geometry;
					auto const& texture_data = nodes_texture_data[i];

					auto vertex_model_to_world = node.get_transform().GetMatrix();

					Node* parent = node.parent;
					while (parent)
					{
						vertex_model_to_world = parent->get_transform().GetMatrix() * vertex_model_to_world;
						parent = parent->parent;
					}

					glUniformMatrix4fv(fill_shadowmap_shader_locations.vertex_model_to_world, 1, GL_FALSE, glm::value_ptr(vertex_model_to_world));

					glUniform1i(fill_shadowmap_shader_locations.has_opacity_texture, texture_data.opacity_texture_id != 0u ? 1 : 0);
					glBindSampler(0u, texture_data.opacity_texture_id != 0u ? samplers[toU(Sampler::Mipmaps)] : samplers[toU(Sampler::Nearest)]);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, texture_data.opacity_texture_id != 0u ? texture_data.opacity_texture_id : debug_texture_id);

					glBindVertexArray(geometry.vao);
					if (geometry.ibo != 0u)
						glDrawElements(geometry.drawing_mode, geometry.indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
					else
						glDrawArrays(geometry.drawing_mode, 0, geometry.vertices_nb);

				}

				glBindTexture(GL_TEXTURE_2D, 0);
				glBindVertexArray(0u);
				glUseProgram(0u);

				glEndQuery(GL_TIME_ELAPSED);
				utils::opengl::debug::endDebugGroup();


				glCullFace(GL_FRONT);
				glEnable(GL_BLEND);
				glDepthFunc(GL_GREATER);
				glDepthMask(GL_FALSE);
				glBlendEquationSeparate(GL_FUNC_ADD, GL_MIN);
				glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
				//
				// Pass 2.2: Accumulate light i contribution
				utils::opengl::debug::beginDebugGroup("Accumulate light " + std::to_string(i));
				glBeginQuery(GL_TIME_ELAPSED, elapsed_time_queries[toU(ElapsedTimeQuery::Light0Accumulation) + i]);

				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[toU(FBO::LightAccumulation)]);
				glUseProgram(accumulate_lights_shader);
				glViewport(0, 0, framebuffer_width, framebuffer_height);

				// XXX: Is any clearing needed?

				glUniform1i(accumulate_light_shader_locations.light_index, static_cast<int>(i));
				glUniformMatrix4fv(accumulate_light_shader_locations.vertex_model_to_world, 1, GL_FALSE, glm::value_ptr(light_world_matrix));
				glUniform3fv(accumulate_light_shader_locations.camera_position, 1, glm::value_ptr(mCamera.mWorld.GetTranslation()));
				glUniform2f(accumulate_light_shader_locations.inverse_screen_resolution,
				            1.0f / static_cast<float>(framebuffer_width),
				            1.0f / static_cast<float>(framebuffer_height));
				glUniform3fv(accumulate_light_shader_locations.light_color, 1, glm::value_ptr(lights[i].color));
				glUniform3fv(accumulate_light_shader_locations.light_position, 1, glm::value_ptr(lightTransform.GetTranslation()));
				glUniform3fv(accumulate_light_shader_locations.light_direction, 1, glm::value_ptr(lightTransform.GetFront()));
				glUniform1f(accumulate_light_shader_locations.light_intensity, lights[i].intensity);
				glUniform1f(accumulate_light_shader_locations.light_angle_falloff, lights[i].angle_falloff);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::DepthBuffer)]);
				glUniform1i(accumulate_light_shader_locations.depth_texture, 0);
				glBindSampler(0, samplers[toU(Sampler::Nearest)]);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::GBufferWorldSpaceNormal)]);
				glUniform1i(accumulate_light_shader_locations.normal_texture, 1);
				glBindSampler(1, samplers[toU(Sampler::Nearest)]);

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::ShadowMap)]);
				glUniform1i(accumulate_light_shader_locations.shadow_texture, 2);
				glBindSampler(2, samplers[toU(Sampler::Shadow)]);

				glBindVertexArray(cone_geometry.vao);
				glDrawArrays(cone_geometry.drawing_mode, 0, cone_geometry.vertices_nb);

				glBindVertexArray(0u);
				glUseProgram(0u);
				glBindSampler(2u, 0u);
				glBindSampler(1u, 0u);
				glBindSampler(0u, 0u);

				glEndQuery(GL_TIME_ELAPSED);
				utils::opengl::debug::endDebugGroup();

				glDepthMask(GL_TRUE);
				glDepthFunc(GL_LESS);
				glDisable(GL_BLEND);
				glCullFace(GL_BACK);
			}


			//
			// Pass 3: Compute final image using both the g-buffer and  the light accumulation buffer
			//
			utils::opengl::debug::beginDebugGroup("Resolve");
			glBeginQuery(GL_TIME_ELAPSED, elapsed_time_queries[toU(ElapsedTimeQuery::Resolve)]);

			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[toU(FBO::Resolve)]);
			glUseProgram(resolve_deferred_shader);
			glViewport(0, 0, framebuffer_width, framebuffer_height);
			// XXX: Is any clearing needed?

			bind_texture_with_sampler(GL_TEXTURE_2D, 0, resolve_deferred_shader, "diffuse_texture", textures[toU(Texture::GBufferDiffuse)], samplers[toU(Sampler::Nearest)]);
			bind_texture_with_sampler(GL_TEXTURE_2D, 1, resolve_deferred_shader, "specular_texture", textures[toU(Texture::GBufferSpecular)], samplers[toU(Sampler::Nearest)]);
			bind_texture_with_sampler(GL_TEXTURE_2D, 2, resolve_deferred_shader, "light_d_texture", textures[toU(Texture::LightDiffuseContribution)], samplers[toU(Sampler::Nearest)]);
			bind_texture_with_sampler(GL_TEXTURE_2D, 3, resolve_deferred_shader, "light_s_texture", textures[toU(Texture::LightSpecularContribution)], samplers[toU(Sampler::Nearest)]);

			bonobo::drawFullscreen();

			glBindSampler(3, 0u);
			glBindSampler(2, 0u);
			glBindSampler(1, 0u);
			glBindSampler(0, 0u);
			glUseProgram(0u);

			glEndQuery(GL_TIME_ELAPSED);
			utils::opengl::debug::endDebugGroup();
		}


		auto const show_debug_elements = show_cone_wireframe || show_basis;
		if (show_debug_elements) {
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[toU(FBO::FinalWithDepth)]);
		}


		//
		// Draw wireframe cones on top of the final image for debugging purposes
		//
		glBeginQuery(GL_TIME_ELAPSED, elapsed_time_queries[toU(ElapsedTimeQuery::ConeWireframe)]);
		if (show_cone_wireframe) {
			utils::opengl::debug::beginDebugGroup("Draw cone wireframe");

			glDisable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			for (size_t i = 0; i < lights.size(); ++i) {
				cone.render(view_projection,
				            lights[i].transform.GetMatrix() * lightOffsetTransform.GetMatrix() * coneScaleTransform.GetMatrix(),
				            render_light_cones_shader, set_uniforms);
			}
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glEnable(GL_CULL_FACE);
			utils::opengl::debug::endDebugGroup();
		}
		glEndQuery(GL_TIME_ELAPSED);


		utils::opengl::debug::beginDebugGroup("Draw GUI");
		glBeginQuery(GL_TIME_ELAPSED, elapsed_time_queries[toU(ElapsedTimeQuery::GUI)]);

		//
		// Display 3D helpers
		//
		if (show_basis) {
			bonobo::renderBasis(basis_thickness_scale, basis_length_scale, mCamera.GetWorldToClipMatrix());
		}

		// If the basis and cone wireframe were not shown, FBO::Resolve
		// is still bound so there is no need to rebind it.
		if (show_debug_elements) {
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[toU(FBO::Resolve)]);
		}

		//
		// Output content of the g-buffer as well as of the shadowmap, for debugging purposes
		//
		//if (show_textures) {
		//	bonobo::displayTexture({-0.95f, -0.95f}, {-0.55f, -0.55f}, textures[toU(Texture::GBufferDiffuse)],            samplers[toU(Sampler::Linear)], {0, 1, 2, -1}, glm::uvec2(framebuffer_width, framebuffer_height));
		//	bonobo::displayTexture({-0.45f, -0.95f}, {-0.05f, -0.55f}, textures[toU(Texture::GBufferSpecular)],           samplers[toU(Sampler::Linear)], {0, 1, 2, -1}, glm::uvec2(framebuffer_width, framebuffer_height));
		//	bonobo::displayTexture({ 0.05f, -0.95f}, { 0.45f, -0.55f}, textures[toU(Texture::GBufferWorldSpaceNormal)],   samplers[toU(Sampler::Linear)], {0, 1, 2, -1}, glm::uvec2(framebuffer_width, framebuffer_height));
		//	bonobo::displayTexture({ 0.55f, -0.95f}, { 0.95f, -0.55f}, textures[toU(Texture::DepthBuffer)],               samplers[toU(Sampler::Linear)], {0, 0, 0, -1}, glm::uvec2(framebuffer_width, framebuffer_height), true, mCamera.mNear, mCamera.mFar);
		//	bonobo::displayTexture({-0.95f,  0.55f}, {-0.55f,  0.95f}, textures[toU(Texture::ShadowMap)],                 samplers[toU(Sampler::Linear)], {0, 0, 0, -1}, glm::uvec2(framebuffer_width, framebuffer_height), true, lightProjectionNearPlane, lightProjectionFarPlane);
		//	bonobo::displayTexture({-0.45f,  0.55f}, {-0.05f,  0.95f}, textures[toU(Texture::LightDiffuseContribution)],  samplers[toU(Sampler::Linear)], {0, 1, 2, -1}, glm::uvec2(framebuffer_width, framebuffer_height));
		//	bonobo::displayTexture({ 0.05f,  0.55f}, { 0.45f,  0.95f}, textures[toU(Texture::LightSpecularContribution)], samplers[toU(Sampler::Linear)], {0, 1, 2, -1}, glm::uvec2(framebuffer_width, framebuffer_height));
		//}

		//
		// Reset viewport back to normal
		//
		glViewport(0, 0, framebuffer_width, framebuffer_height);
		bool opened;
		opened = ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_None);
		if (opened) {
			ImGui::Checkbox("Pause lights", &are_lights_paused);
			//ImGui::SliderInt("Number of lights", &lights_nb, 1, static_cast<int>(constant::lights_nb));
			ImGui::Checkbox("Show textures", &show_textures);
			ImGui::Checkbox("Show light cones wireframe", &show_cone_wireframe);
			ImGui::Separator();
			ImGui::Checkbox("Show basis", &show_basis);
			ImGui::SliderFloat("Basis thickness scale", &basis_thickness_scale, 0.0f, 100.0f);
			ImGui::SliderFloat("Basis length scale", &basis_length_scale, 0.0f, 100.0f);
			ImGui::SliderFloat("Sun intensity", &(lights[0].intensity), 0.0f, 20000.0f);
			ImGui::SliderFloat("Plane speed", &(airplane.move_speed), 5.0f, 25.0f);
			ImGui::SliderFloat("Tilt", &tilt_amount, 0.0f, 1.0f);
			ImGui::SliderFloat("Catch up", &catch_up, 0.5f, 2.5f);
			ImGui::Checkbox("Track plane", &track_plane);
			ImGui::Checkbox("Smooted camera", &smoothed_camera);
		}
		ImGui::End();

		if (show_logs)
			Log::View::Render();
		mWindowManager.RenderImGuiFrame(show_gui);

		glEndQuery(GL_TIME_ELAPSED);
		utils::opengl::debug::endDebugGroup();

		//
		// Blit the result back to the default framebuffer.
		//
		utils::opengl::debug::beginDebugGroup("Copy to default framebuffer");
		glBeginQuery(GL_TIME_ELAPSED, elapsed_time_queries[toU(ElapsedTimeQuery::CopyToFramebuffer)]);

		// FBO::Resolve has already been bound to GL_READ_FRAMEBUFFER before rendering the first frame,
		// as no other frame buffer gets bound to it.
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0u);
		glBlitFramebuffer(0, 0, framebuffer_width, framebuffer_height, 0, 0, framebuffer_width, framebuffer_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		glEndQuery(GL_TIME_ELAPSED);
		utils::opengl::debug::endDebugGroup();

		glfwSwapBuffers(window);

		first_frame = false;
	}

	glDeleteBuffers(static_cast<GLsizei>(ubos.size()), ubos.data());
	glDeleteQueries(static_cast<GLsizei>(elapsed_time_queries.size()), elapsed_time_queries.data());
	glDeleteSamplers(static_cast<GLsizei>(samplers.size()), samplers.data());
	glDeleteFramebuffers(static_cast<GLsizei>(fbos.size()), fbos.data());
	glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());

	glDeleteProgram(resolve_deferred_shader);
	resolve_deferred_shader = 0u;
	glDeleteProgram(accumulate_lights_shader);
	accumulate_lights_shader = 0u;
	glDeleteProgram(fill_shadowmap_shader);
	fill_shadowmap_shader = 0u;
	glDeleteProgram(fill_gbuffer_shader);
	fill_gbuffer_shader = 0u;
	glDeleteProgram(fallback_shader);
	fallback_shader = 0u;
}

int main()
{
	std::setlocale(LC_ALL, "");

	Bonobo framework;

	try {
		edan35::Assignment2 assignment2(framework.GetWindowManager());
		assignment2.run();
	} catch (std::runtime_error const& e) {
		LogError(e.what());
	}
}

namespace
{
Textures createTextures(GLsizei framebuffer_width, GLsizei framebuffer_height)
{
	Textures textures;
	glGenTextures(static_cast<GLsizei>(textures.size()), textures.data());

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::DepthBuffer)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, framebuffer_width, framebuffer_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::DepthBuffer)], "Depth buffer");

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::ShadowMap)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, constant::shadowmap_res_x, constant::shadowmap_res_y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::ShadowMap)], "Shadow map");

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::GBufferDiffuse)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::GBufferDiffuse)], "GBuffer diffuse");

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::GBufferSpecular)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::GBufferSpecular)], "GBuffer specular");

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::GBufferWorldSpaceNormal)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::GBufferWorldSpaceNormal)], "GBuffer normals");

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::LightDiffuseContribution)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::LightDiffuseContribution)], "Light diffuse contribution");

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::LightSpecularContribution)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::LightSpecularContribution)], "Light specular contribution");

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::Result)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::Result)], "Final result");

	glBindTexture(GL_TEXTURE_2D, textures[toU(Texture::Flood)]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, constant::earth_res_x, constant::earth_res_y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	utils::opengl::debug::nameObject(GL_TEXTURE, textures[toU(Texture::Flood)], "JumpFlood");

	glBindTexture(GL_TEXTURE_2D, 0u);
	return textures;
}

Samplers createSamplers()
{
	Samplers samplers;
	glGenSamplers(static_cast<GLsizei>(samplers.size()), samplers.data());

	// For sampling 2-D textures without interpolation.
	glSamplerParameteri(samplers[toU(Sampler::Nearest)], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(samplers[toU(Sampler::Nearest)], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	utils::opengl::debug::nameObject(GL_SAMPLER, samplers[toU(Sampler::Nearest)], "Nearest");

	// For sampling 2-D textures without mipmaps.
	glSamplerParameteri(samplers[toU(Sampler::Linear)], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(samplers[toU(Sampler::Linear)], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	utils::opengl::debug::nameObject(GL_SAMPLER, samplers[toU(Sampler::Linear)], "Linear");

	// For sampling 2-D textures with mipmaps.
	glSamplerParameteri(samplers[toU(Sampler::Mipmaps)], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(samplers[toU(Sampler::Mipmaps)], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	utils::opengl::debug::nameObject(GL_SAMPLER, samplers[toU(Sampler::Mipmaps)], "Mimaps");

	// For sampling 2-D shadow maps
	glSamplerParameteri(samplers[toU(Sampler::Shadow)], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(samplers[toU(Sampler::Shadow)], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(samplers[toU(Sampler::Shadow)], GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glSamplerParameteri(samplers[toU(Sampler::Shadow)], GL_TEXTURE_COMPARE_FUNC, GL_LESS);
	utils::opengl::debug::nameObject(GL_SAMPLER, samplers[toU(Sampler::Shadow)], "Shadow");

	return samplers;
}

FBOs createFramebufferObjects(Textures const& textures)
{
	auto const validate_fbo = [](std::string const& fbo_name){
		auto const status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status == GL_FRAMEBUFFER_COMPLETE)
			return;

		LogError("Framebuffer \"%s\" is not complete: check the logs for additional information.", fbo_name.data());
	};

	FBOs fbos;
	glGenFramebuffers(static_cast<GLsizei>(fbos.size()), fbos.data());

	glBindFramebuffer(GL_FRAMEBUFFER, fbos[toU(FBO::JumpFlood)]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[toU(Texture::Flood)], 0);

	std::array<GLenum, 3> const flood_buffer_draws = {
		GL_COLOR_ATTACHMENT0, // The fragment shader output at location 0 will be written to colour attachment 0 (i.e. the flood texture).
	};
	glDrawBuffers(static_cast<GLsizei>(flood_buffer_draws.size()), flood_buffer_draws.data());
	validate_fbo("FloodBuffer");
	utils::opengl::debug::nameObject(GL_FRAMEBUFFER, fbos[toU(FBO::JumpFlood)], "FloodBuffer");

	glBindFramebuffer(GL_FRAMEBUFFER, fbos[toU(FBO::GBuffer)]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[toU(Texture::GBufferDiffuse)], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textures[toU(Texture::GBufferSpecular)], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, textures[toU(Texture::GBufferWorldSpaceNormal)], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[toU(Texture::DepthBuffer)], 0);
	glReadBuffer(GL_NONE); // Disable reading back from the colour attachments, as unnecessary in this assignment.
	// Configure the mapping from fragment shader outputs to colour attachments.
	std::array<GLenum, 3> const gbuffer_draws = {
		GL_COLOR_ATTACHMENT0, // The fragment shader output at location 0 will be written to colour attachment 0 (i.e. the diffuse texture).
		GL_COLOR_ATTACHMENT1, // The fragment shader output at location 1 will be written to colour attachment 1 (i.e. the specular texture).
		GL_COLOR_ATTACHMENT2  // The fragment shader output at location 2 will be written to colour attachment 2 (i.e. the normal texture).
	};
	glDrawBuffers(static_cast<GLsizei>(gbuffer_draws.size()), gbuffer_draws.data());
	validate_fbo("GBuffer");
	utils::opengl::debug::nameObject(GL_FRAMEBUFFER, fbos[toU(FBO::GBuffer)], "GBuffer");

	glBindFramebuffer(GL_FRAMEBUFFER, fbos[toU(FBO::ShadowMap)]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[toU(Texture::ShadowMap)], 0);
	validate_fbo("Shadow map generation");
	utils::opengl::debug::nameObject(GL_FRAMEBUFFER, fbos[toU(FBO::ShadowMap)], "Shadow map generation");

	glBindFramebuffer(GL_FRAMEBUFFER, fbos[toU(FBO::LightAccumulation)]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[toU(Texture::LightDiffuseContribution)], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textures[toU(Texture::LightSpecularContribution)], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[toU(Texture::DepthBuffer)], 0);
	glReadBuffer(GL_NONE); // Disable reading back from the colour attachments, as unnecessary in this assignment.
	// Configure the mapping from fragment shader outputs to colour attachments.
	std::array<GLenum, 2> const light_accumulation_draws = {
		GL_COLOR_ATTACHMENT0, // The fragment shader output at location 0 will be written to colour attachment 0 (i.e. the light diffuse contribution texture).
		GL_COLOR_ATTACHMENT1  // The fragment shader output at location 1 will be written to colour attachment 1 (i.e. the light specular contribution texture).
	};
	glDrawBuffers(static_cast<GLsizei>(light_accumulation_draws.size()), light_accumulation_draws.data());
	validate_fbo("Light accumulation");
	utils::opengl::debug::nameObject(GL_FRAMEBUFFER, fbos[toU(FBO::LightAccumulation)], "Light acccumulation");

	glBindFramebuffer(GL_FRAMEBUFFER, fbos[toU(FBO::Resolve)]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[toU(Texture::Result)], 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0); // Colour attachment result 0 (i.e. the rendering result texture) will be blitted to the screen.
	glDrawBuffer(GL_COLOR_ATTACHMENT0); // The fragment shader output at location 0 will be written to colour attachment 0 (i.e. the rendering result texture).
	validate_fbo("Resolve");
	utils::opengl::debug::nameObject(GL_FRAMEBUFFER, fbos[toU(FBO::Resolve)], "Resolve");

	glBindFramebuffer(GL_FRAMEBUFFER, fbos[toU(FBO::FinalWithDepth)]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[toU(Texture::Result)], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[toU(Texture::DepthBuffer)], 0);
	glReadBuffer(GL_NONE); // Disable reading back from the colour attachments, as unnecessary in this assignment.
	glDrawBuffer(GL_COLOR_ATTACHMENT0); // The fragment shader output at location 0 will be written to colour attachment 0 (i.e. the rendering result texture).
	validate_fbo("Final with depth");
	utils::opengl::debug::nameObject(GL_FRAMEBUFFER, fbos[toU(FBO::FinalWithDepth)], "Cone wireframe");

	glBindFramebuffer(GL_FRAMEBUFFER, 0u);
	return fbos;
}

ElapsedTimeQueries createElapsedTimeQueries()
{
	ElapsedTimeQueries queries;
	glGenQueries(static_cast<GLsizei>(queries.size()), queries.data());

	if (utils::opengl::debug::isSupported())
	{
		// Queries (like any other OpenGL object) need to have been used at least
		// once to ensure their resources have been allocated so we can call
		// `glObjectLabel()` on them.
		auto const register_query = [](GLuint const query) {
			glBeginQuery(GL_TIME_ELAPSED, query);
			glEndQuery(GL_TIME_ELAPSED);
		};

		register_query(queries[toU(ElapsedTimeQuery::GbufferGeneration)]);
		utils::opengl::debug::nameObject(GL_QUERY, queries[toU(ElapsedTimeQuery::GbufferGeneration)], "GBuffer generation");

		for (size_t i = 0; i < constant::lights_nb; ++i)
		{
			register_query(queries[toU(ElapsedTimeQuery::ShadowMap0Generation) + i]);
			utils::opengl::debug::nameObject(GL_QUERY, queries[toU(ElapsedTimeQuery::ShadowMap0Generation) + i], "Shadow map " + std::to_string(i) + " generation");

			register_query(queries[toU(ElapsedTimeQuery::Light0Accumulation) + i]);
			utils::opengl::debug::nameObject(GL_QUERY, queries[toU(ElapsedTimeQuery::Light0Accumulation) + i], "Light" + std::to_string(i) + " accumulation");
		}

		register_query(queries[toU(ElapsedTimeQuery::Resolve)]);
		utils::opengl::debug::nameObject(GL_QUERY, queries[toU(ElapsedTimeQuery::Resolve)], "Resolve");

		register_query(queries[toU(ElapsedTimeQuery::ConeWireframe)]);
		utils::opengl::debug::nameObject(GL_QUERY, queries[toU(ElapsedTimeQuery::ConeWireframe)], "Cone wireframe");

		register_query(queries[toU(ElapsedTimeQuery::GUI)]);
		utils::opengl::debug::nameObject(GL_QUERY, queries[toU(ElapsedTimeQuery::GUI)], "GUI");
	}

	return queries;
}

UBOs createUniformBufferObjects()
{
	UBOs ubos;
	glGenBuffers(static_cast<GLsizei>(ubos.size()), ubos.data());

	glBindBuffer(GL_UNIFORM_BUFFER, ubos[toU(UBO::CameraViewProjTransforms)]);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(ViewProjTransforms), nullptr, GL_STREAM_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, toU(UBO::CameraViewProjTransforms), ubos[toU(UBO::CameraViewProjTransforms)]);
	utils::opengl::debug::nameObject(GL_BUFFER, ubos[toU(UBO::CameraViewProjTransforms)], "Camera view-projection transforms");

	glBindBuffer(GL_UNIFORM_BUFFER, ubos[toU(UBO::LightViewProjTransforms)]);
	glBufferData(GL_UNIFORM_BUFFER, constant::lights_nb * sizeof(ViewProjTransforms), nullptr, GL_STREAM_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, toU(UBO::LightViewProjTransforms), ubos[toU(UBO::LightViewProjTransforms)]);
	utils::opengl::debug::nameObject(GL_BUFFER, ubos[toU(UBO::LightViewProjTransforms)], "Light view-projection transforms");

	glBindBuffer(GL_UNIFORM_BUFFER, 0u);
	return ubos;
}

void fillGBufferShaderLocations(GLuint gbuffer_shader, GBufferShaderLocations& locations)
{
	locations.ubo_CameraViewProjTransforms = glGetUniformBlockIndex(gbuffer_shader, "CameraViewProjTransforms");
	locations.vertex_model_to_world = glGetUniformLocation(gbuffer_shader, "vertex_model_to_world");
	locations.normal_model_to_world = glGetUniformLocation(gbuffer_shader, "normal_model_to_world");
	locations.diffuse_texture = glGetUniformLocation(gbuffer_shader, "diffuse_texture");
	locations.specular_texture = glGetUniformLocation(gbuffer_shader, "specular_texture");
	locations.normals_texture = glGetUniformLocation(gbuffer_shader, "normals_texture");
	locations.opacity_texture = glGetUniformLocation(gbuffer_shader, "opacity_texture");
	locations.has_diffuse_texture = glGetUniformLocation(gbuffer_shader, "has_diffuse_texture");
	locations.has_specular_texture = glGetUniformLocation(gbuffer_shader, "has_specular_texture");
	locations.has_normals_texture = glGetUniformLocation(gbuffer_shader, "has_normals_texture");
	locations.has_opacity_texture = glGetUniformLocation(gbuffer_shader, "has_opacity_texture");
	locations.height_texture = glGetUniformLocation(gbuffer_shader, "height_texture");
	locations.earth_normal_texture = glGetUniformLocation(gbuffer_shader, "earth_normal_texture");


	locations.has_waves_texture = glGetUniformLocation(gbuffer_shader, "has_waves_texture");
	locations.waves_texture1 = glGetUniformLocation(gbuffer_shader, "waves_texture1");
	locations.waves_texture2 = glGetUniformLocation(gbuffer_shader, "waves_texture2");
	locations.foam_texture = glGetUniformLocation(gbuffer_shader, "foam_texture");

	locations.elapsed_time = glGetUniformLocation(gbuffer_shader, "elapsed_time");


	glUniformBlockBinding(gbuffer_shader, locations.ubo_CameraViewProjTransforms, toU(UBO::CameraViewProjTransforms));

}

void fillShadowmapShaderLocations(GLuint shadowmap_shader, FillShadowmapShaderLocations& locations)
{
	locations.ubo_LightViewProjTransforms = glGetUniformBlockIndex(shadowmap_shader, "LightViewProjTransforms");
	locations.light_index = glGetUniformLocation(shadowmap_shader, "light_index");
	locations.vertex_model_to_world = glGetUniformLocation(shadowmap_shader, "vertex_model_to_world");
	locations.opacity_texture = glGetUniformLocation(shadowmap_shader, "opacity_texture");
	locations.has_opacity_texture = glGetUniformLocation(shadowmap_shader, "has_opacity_texture");

	glUniformBlockBinding(shadowmap_shader, locations.ubo_LightViewProjTransforms, toU(UBO::LightViewProjTransforms));
}

void fillAccumulateLightsShaderLocations(GLuint accumulate_lights_shader, AccumulateLightsShaderLocations& locations)
{
	locations.ubo_CameraViewProjTransforms = glGetUniformBlockIndex(accumulate_lights_shader, "CameraViewProjTransforms");
	locations.ubo_LightViewProjTransforms = glGetUniformBlockIndex(accumulate_lights_shader, "LightViewProjTransforms");
	locations.light_index = glGetUniformLocation(accumulate_lights_shader, "light_index");
	locations.vertex_model_to_world = glGetUniformLocation(accumulate_lights_shader, "vertex_model_to_world");
	locations.vertex_world_to_clip = glGetUniformLocation(accumulate_lights_shader, "vertex_world_to_clip");
	locations.vertex_clip_to_world = glGetUniformLocation(accumulate_lights_shader, "vertex_clip_to_world");
	locations.depth_texture = glGetUniformLocation(accumulate_lights_shader, "depth_texture");
	locations.normal_texture = glGetUniformLocation(accumulate_lights_shader, "normal_texture");
	locations.shadow_texture = glGetUniformLocation(accumulate_lights_shader, "shadow_texture");
	locations.camera_position = glGetUniformLocation(accumulate_lights_shader, "camera_position");
	locations.inverse_screen_resolution = glGetUniformLocation(accumulate_lights_shader, "inverse_screen_resolution");
	locations.light_color = glGetUniformLocation(accumulate_lights_shader, "light_color");
	locations.light_position = glGetUniformLocation(accumulate_lights_shader, "light_position");
	locations.light_direction = glGetUniformLocation(accumulate_lights_shader, "light_direction");
	locations.light_intensity = glGetUniformLocation(accumulate_lights_shader, "light_intensity");
	locations.light_angle_falloff = glGetUniformLocation(accumulate_lights_shader, "light_angle_falloff");

	glUniformBlockBinding(accumulate_lights_shader, locations.ubo_CameraViewProjTransforms, toU(UBO::CameraViewProjTransforms));
	glUniformBlockBinding(accumulate_lights_shader, locations.ubo_LightViewProjTransforms, toU(UBO::LightViewProjTransforms));
}

bonobo::mesh_data
loadCone()
{
	bonobo::mesh_data cone;
	cone.vertices_nb = 65;
	cone.drawing_mode = GL_TRIANGLE_STRIP;
	float vertexArrayData[65 * 3] = {
		0.f, 1.f, -1.f,
		0.f, 0.f, 0.f,
		0.38268f, 0.92388f, -1.f,
		0.f, 0.f, 0.f,
		0.70711f, 0.70711f, -1.f,
		0.f, 0.f, 0.f,
		0.92388f, 0.38268f, -1.f,
		0.f, 0.f, 0.f,
		1.f, 0.f, -1.f,
		0.f, 0.f, 0.f,
		0.92388f, -0.38268f, -1.f,
		0.f, 0.f, 0.f,
		0.70711f, -0.70711f, -1.f,
		0.f, 0.f, 0.f,
		0.38268f, -0.92388f, -1.f,
		0.f, 0.f, 0.f,
		0.f, -1.f, -1.f,
		0.f, 0.f, 0.f,
		-0.38268f, -0.92388f, -1.f,
		0.f, 0.f, 0.f,
		-0.70711f, -0.70711f, -1.f,
		0.f, 0.f, 0.f,
		-0.92388f, -0.38268f, -1.f,
		0.f, 0.f, 0.f,
		-1.f, 0.f, -1.f,
		0.f, 0.f, 0.f,
		-0.92388f, 0.38268f, -1.f,
		0.f, 0.f, 0.f,
		-0.70711f, 0.70711f, -1.f,
		0.f, 0.f, 0.f,
		-0.38268f, 0.92388f, -1.f,
		0.f, 1.f, -1.f,
		0.f, 1.f, -1.f,
		0.38268f, 0.92388f, -1.f,
		0.f, 1.f, -1.f,
		0.70711f, 0.70711f, -1.f,
		0.f, 0.f, -1.f,
		0.92388f, 0.38268f, -1.f,
		0.f, 0.f, -1.f,
		1.f, 0.f, -1.f,
		0.f, 0.f, -1.f,
		0.92388f, -0.38268f, -1.f,
		0.f, 0.f, -1.f,
		0.70711f, -0.70711f, -1.f,
		0.f, 0.f, -1.f,
		0.38268f, -0.92388f, -1.f,
		0.f, 0.f, -1.f,
		0.f, -1.f, -1.f,
		0.f, 0.f, -1.f,
		-0.38268f, -0.92388f, -1.f,
		0.f, 0.f, -1.f,
		-0.70711f, -0.70711f, -1.f,
		0.f, 0.f, -1.f,
		-0.92388f, -0.38268f, -1.f,
		0.f, 0.f, -1.f,
		-1.f, 0.f, -1.f,
		0.f, 0.f, -1.f,
		-0.92388f, 0.38268f, -1.f,
		0.f, 0.f, -1.f,
		-0.70711f, 0.70711f, -1.f,
		0.f, 0.f, -1.f,
		-0.38268f, 0.92388f, -1.f,
		0.f, 0.f, -1.f,
		0.f, 1.f, -1.f,
		0.f, 0.f, -1.f
	};

	glGenVertexArrays(1, &cone.vao);
	assert(cone.vao != 0u);
	glBindVertexArray(cone.vao);
	{
		utils::opengl::debug::nameObject(GL_VERTEX_ARRAY, cone.vao, "Cone VAO");

		glGenBuffers(1, &cone.bo);
		assert(cone.bo != 0u);
		glBindBuffer(GL_ARRAY_BUFFER, cone.bo);
		glBufferData(GL_ARRAY_BUFFER, cone.vertices_nb * 3 * sizeof(float), vertexArrayData, GL_STATIC_DRAW);
		utils::opengl::debug::nameObject(GL_BUFFER, cone.bo, "Cone VBO");

		glVertexAttribPointer(static_cast<int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));
		glEnableVertexAttribArray(static_cast<int>(bonobo::shader_bindings::vertices));

		glBindBuffer(GL_ARRAY_BUFFER, 0u);
	}
	glBindVertexArray(0u);

	return cone;
}
} // namespace
