#include "parametric_shapes.hpp"
#include "core/Log.h"

#include <glm/glm.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

bonobo::mesh_data
parametric_shapes::createLine(glm::vec3 a, glm::vec3 b, float width)
{
	std::array<glm::vec3, 2> vertices{a, b};

	bonobo::mesh_data data;

	glGenVertexArrays(1, &data.vao);
	glBindVertexArray(data.vao);

	data.drawing_mode = GL_LINES;
	glLineWidth(width);

	data.vertices_nb = 2;

	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));

	glGenBuffers(1, &data.bo);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, vertices_size, &vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));


	glBindVertexArray(0u);
	glBindBuffer(GL_ARRAY_BUFFER, 0u);

	return data;
}


bonobo::mesh_data
parametric_shapes::createQuad(float const width, float const height,
                              unsigned int const horizontal_split_count,
                              unsigned int const vertical_split_count)
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> texcoords;
	std::vector<glm::uvec3> index_sets;

	if (horizontal_split_count > 0u && vertical_split_count > 0u) {
		const unsigned int horizontal_edge_count = horizontal_split_count + 1u;
		const unsigned int vertical_edge_count = vertical_split_count + 1u;

		const unsigned int horizontal_vertex_count = horizontal_edge_count + 1u;
		const unsigned int vertical_vertex_count = vertical_edge_count + 1u;

		const float delta_width = width / static_cast<float>(horizontal_edge_count);
		const float delta_height = height / static_cast<float>(vertical_edge_count);

		const size_t total_vertex_count = horizontal_vertex_count * vertical_vertex_count;

		vertices = std::vector<glm::vec3>(total_vertex_count);
		texcoords = std::vector<glm::vec3>(total_vertex_count);
		index_sets = std::vector<glm::uvec3>(2u * horizontal_edge_count * vertical_edge_count);

		int i, j;
		float current_x = 0.0f;
		float current_z = 0.0f;
		size_t index = 0u;

		//calculate vertices
		for (i = 0; i < horizontal_vertex_count; i++) {
			for (j = 0; j < vertical_vertex_count; j++) {
				vertices[index] = glm::vec3(current_x, 0.0f, current_z);

				// texture coordinates
				texcoords[index] = glm::vec3(current_x/width, current_z/height, 0.0f);



				current_z += delta_height;
				index++;
			}

			current_z = 0.0f;
			current_x += delta_width;
		}


		index = 0u;
		unsigned int current_vi;

		//calculate triangles
		for (i = 0; i < horizontal_edge_count; i++) {
			for (j = 0; j < vertical_edge_count; j++) {
				current_vi = i * vertical_vertex_count + j;

				index_sets[index] = glm::vec3(current_vi, current_vi + vertical_vertex_count + 1u, current_vi + vertical_vertex_count);
				index++;

				index_sets[index] = glm::vec3(current_vi, current_vi + 1u, current_vi + vertical_vertex_count + 1u);
				index++;
			}
		}
	}
	else {
		vertices = std::vector<glm::vec3>{ glm::vec3(0.0f, 0.0f, 0.0f),
										   glm::vec3(width, 0.0f, 0.0f),
										   glm::vec3(width, 0.0f, height),
										   glm::vec3(0.0f, 0.0f, height) };

		index_sets = std::vector<glm::uvec3>{ glm::uvec3(0u, 1u, 2u),
											  glm::uvec3(0u, 2u, 3u) };
	}

	bonobo::mesh_data data;

	//if (horizontal_split_count > 0u || vertical_split_count > 0u)
	//{
	//	LogError("parametric_shapes::createQuad() does not support tesselation.");
	//	return data;
	//}

	glGenVertexArrays(1, &data.vao);
	glBindVertexArray(data.vao);


	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
	auto const texcoords_offset = vertices_size;
	auto const texcoords_size = static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
	auto const bo_size = static_cast<GLsizeiptr>(vertices_size + texcoords_size);

	glGenBuffers(1, &data.bo);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0u, vertices_size, static_cast<GLvoid const*>(vertices.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

	glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(texcoords.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));





	// Now, let's allocate a second one for the indices.
	//
	// Have the buffer's name stored into `data.ibo`.
	glGenBuffers(1, &data.ibo);

	// We still want a 1D-array, but this time it should be a 1D-array of
	// elements, aka. indices!
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, /*! \todo how many bytes should the buffer contain? */index_sets.size() * sizeof(glm::uvec3),
	             /* where is the data stored on the CPU? */index_sets.data(),
	             /* inform OpenGL that the data is modified once, but used often */GL_STATIC_DRAW);

	data.indices_nb = /*! \todo how many indices do we have? */index_sets.size() * 3;

	// All the data has been recorded, we can unbind them.
	glBindVertexArray(0u);
	glBindBuffer(GL_ARRAY_BUFFER, 0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

	return data;
}

bonobo::mesh_data
parametric_shapes::createSphere(float const radius,
                                unsigned int const longitude_split_count,
                                unsigned int const latitude_split_count)
{
	auto const longitude_edge_count = longitude_split_count + 1u;
	auto const latitude_edge_count = latitude_split_count + 1u;

	auto const longitude_vertex_count = longitude_edge_count + 1u;
	auto const latitude_vertex_count = latitude_edge_count + 1u;

	auto const vertices_nb = longitude_vertex_count * latitude_vertex_count;

	auto vertices = std::vector<glm::vec3>(vertices_nb);
	auto normals = std::vector<glm::vec3>(vertices_nb);
	auto texcoords = std::vector<glm::vec3>(vertices_nb);
	auto tangents = std::vector<glm::vec3>(vertices_nb);
	auto binormals = std::vector<glm::vec3>(vertices_nb);

	const float d_theta = glm::two_pi<float>() / longitude_edge_count;
	const float d_phi = glm::pi<float>() / latitude_edge_count;

	size_t index = 0u;

	for (unsigned int i = 0; i < longitude_vertex_count; i++) {
		float theta = d_theta * i;
		float sin_theta = glm::sin(theta);
		float cos_theta = glm::cos(theta);

		for (unsigned int j = 0; j < latitude_vertex_count; j++) {
			float phi = d_phi * j;
			float sin_phi = glm::sin(phi);
			float cos_phi = glm::cos(phi);

			//vertex
			vertices[index] = glm::vec3(radius * sin_theta * sin_phi,
										-radius * cos_phi,
										radius * cos_theta * sin_phi);

			// texture coordinates
			texcoords[index] = glm::vec3(static_cast<float>(i) / (static_cast<float>(longitude_vertex_count)),
										 static_cast<float>(j) / (static_cast<float>(latitude_vertex_count)),
										 0.0f);

			//tangent
			tangents[index] = glm::vec3(cos_theta,
										0,
										-sin_theta);

			//binormal
			binormals[index] = glm::vec3(sin_theta * cos_phi,
										 sin_phi,
										 cos_theta * cos_phi);

			//normal
			normals[index] = glm::cross(tangents[index], binormals[index]);

			index++;
		}
	}

	auto index_sets = std::vector<glm::uvec3>(2u * longitude_edge_count * latitude_edge_count);

	index = 0u;
	for (unsigned int i = 0u; i < longitude_edge_count; ++i) {

		for (unsigned int j = 0u; j < latitude_edge_count; ++j) {
			index_sets[index] = glm::uvec3(latitude_vertex_count * (i + 0u) + (j + 0u),
										   latitude_vertex_count * (i + 1u) + (j + 1u),
										   latitude_vertex_count * (i + 0u) + (j + 1u));

			++index;

			index_sets[index] = glm::uvec3(latitude_vertex_count * (i + 0u) + (j + 0u),
										   latitude_vertex_count * (i + 1u) + (j + 0u),
										   latitude_vertex_count * (i + 1u) + (j + 1u));

			++index;
		}
	}



	bonobo::mesh_data data;

	//What does this do?
	glGenVertexArrays(1, &data.vao);
	glBindVertexArray(data.vao);

	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
	auto const normals_offset = vertices_size;
	auto const normals_size = static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
	auto const texcoords_offset = normals_offset + normals_size;
	auto const texcoords_size = static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
	auto const tangents_offset = texcoords_offset + texcoords_size;
	auto const tangents_size = static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
	auto const binormals_offset = tangents_offset + tangents_size;
	auto const binormals_size = static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
	auto const bo_size = static_cast<GLsizeiptr>(vertices_size
		+ normals_size
		+ texcoords_size
		+ tangents_size
		+ binormals_size
		);

	glGenBuffers(1, &data.bo);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0u, vertices_size, static_cast<GLvoid const*>(vertices.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

	glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size, static_cast<GLvoid const*>(normals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::normals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(normals_offset));

	glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(texcoords.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));

	glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size, static_cast<GLvoid const*>(tangents.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::tangents));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(tangents_offset));

	glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size, static_cast<GLvoid const*>(binormals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::binormals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(binormals_offset));

	//Why element array?
	glGenBuffers(1, &data.ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(index_sets.size() * sizeof(glm::uvec3)), reinterpret_cast<GLvoid const*>(index_sets.data()), GL_STATIC_DRAW);

	data.indices_nb = index_sets.size() * 3;


	glBindVertexArray(0u);
	glBindBuffer(GL_ARRAY_BUFFER, 0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

	return data;
}

bonobo::mesh_data
parametric_shapes::createTorus(float const major_radius,
                               float const minor_radius,
                               unsigned int const major_split_count,
                               unsigned int const minor_split_count)
{
	auto const major_edges_count = major_split_count + 1u;
	auto const minor_edges_count = minor_split_count + 1u;
	auto const major_vertices_count = major_edges_count + 1u;
	auto const minor_vertices_count = minor_edges_count + 1u;
	auto const vertices_nb = major_vertices_count * minor_vertices_count;

	auto vertices = std::vector<glm::vec3>(vertices_nb);
	auto normals = std::vector<glm::vec3>(vertices_nb);
	auto texcoords = std::vector<glm::vec3>(vertices_nb);
	auto tangents = std::vector<glm::vec3>(vertices_nb);
	auto binormals = std::vector<glm::vec3>(vertices_nb);

	const float d_theta = glm::two_pi<float>() / minor_edges_count;
	const float d_phi = glm::two_pi<float>() / major_edges_count;

	// generate vertices iteratively
	size_t index = 0u;
	float theta = 0.0f;
	for (unsigned int i = 0u; i < major_vertices_count; ++i) {
		float phi = d_phi * i;
		float sin_phi = glm::sin(phi);
		float cos_phi = glm::cos(phi);

		for (unsigned int j = 0; j < minor_vertices_count; j++) {
			float theta = d_theta * j;
			float sin_theta = glm::sin(theta);
			float cos_theta = glm::cos(theta);

			//vertex
			vertices[index] = glm::vec3((major_radius + minor_radius * cos_theta) * cos_phi,
										-minor_radius * sin_theta,
										(major_radius + minor_radius * cos_theta) * sin_phi);

			// texture coordinates
			texcoords[index] = glm::vec3(static_cast<float>(i) / (static_cast<float>(major_vertices_count)),
										 static_cast<float>(j) / (static_cast<float>(minor_vertices_count)),
										 0.0f);

			//tangent
			tangents[index] = glm::vec3(sin_theta * cos_phi,
										cos_theta,
										sin_theta * sin_phi);

			//binormal
			binormals[index] = glm::normalize(glm::vec3(-(major_radius + minor_radius * cos_theta) * sin_phi,
										 0,
										 (major_radius + minor_radius * cos_theta) * cos_phi));

			//normal
			normals[index] = glm::cross(tangents[index], binormals[index]);

			index++;
		}


	}
	auto index_sets = std::vector<glm::uvec3>(2u * major_edges_count * minor_edges_count);

	index = 0u;
	for (unsigned int i = 0u; i < major_edges_count; ++i) {

		for (unsigned int j = 0u; j < minor_edges_count; ++j) {
			index_sets[index] = glm::uvec3(minor_vertices_count * (i + 0u) + (j + 0u),
										   minor_vertices_count * (i + 1u) + (j + 1u),
										   minor_vertices_count * (i + 0u) + (j + 1u));

			++index;

			index_sets[index] = glm::uvec3(minor_vertices_count * (i + 0u) + (j + 0u),
										   minor_vertices_count * (i + 1u) + (j + 0u),
										   minor_vertices_count * (i + 1u) + (j + 1u));

			++index;
		}
	}



	bonobo::mesh_data data;

	//What does this do?
	glGenVertexArrays(1, &data.vao);
	glBindVertexArray(data.vao);

	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
	auto const normals_offset = vertices_size;
	auto const normals_size = static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
	auto const texcoords_offset = normals_offset + normals_size;
	auto const texcoords_size = static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
	auto const tangents_offset = texcoords_offset + texcoords_size;
	auto const tangents_size = static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
	auto const binormals_offset = tangents_offset + tangents_size;
	auto const binormals_size = static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
	auto const bo_size = static_cast<GLsizeiptr>(vertices_size
		+ normals_size
		+ texcoords_size
		+ tangents_size
		+ binormals_size
		);

	glGenBuffers(1, &data.bo);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0u, vertices_size, static_cast<GLvoid const*>(vertices.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

	glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size, static_cast<GLvoid const*>(normals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::normals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(normals_offset));

	glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(texcoords.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));

	glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size, static_cast<GLvoid const*>(tangents.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::tangents));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(tangents_offset));

	glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size, static_cast<GLvoid const*>(binormals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::binormals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(binormals_offset));

	//Why element array?
	glGenBuffers(1, &data.ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(index_sets.size() * sizeof(glm::uvec3)), reinterpret_cast<GLvoid const*>(index_sets.data()), GL_STATIC_DRAW);

	data.indices_nb = index_sets.size() * 3;


	glBindVertexArray(0u);
	glBindBuffer(GL_ARRAY_BUFFER, 0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

	return data;
}

bonobo::mesh_data
parametric_shapes::createCircleRing(float const radius,
                                    float const spread_length,
                                    unsigned int const circle_split_count,
                                    unsigned int const spread_split_count)
{
	auto const circle_slice_edges_count = circle_split_count + 1u;
	auto const spread_slice_edges_count = spread_split_count + 1u;
	auto const circle_slice_vertices_count = circle_slice_edges_count + 1u;
	auto const spread_slice_vertices_count = spread_slice_edges_count + 1u;
	auto const vertices_nb = circle_slice_vertices_count * spread_slice_vertices_count;

	auto vertices  = std::vector<glm::vec3>(vertices_nb);
	auto normals   = std::vector<glm::vec3>(vertices_nb);
	auto texcoords = std::vector<glm::vec3>(vertices_nb);
	auto tangents  = std::vector<glm::vec3>(vertices_nb);
	auto binormals = std::vector<glm::vec3>(vertices_nb);

	float const spread_start = radius - 0.5f * spread_length;
	float const d_theta = glm::two_pi<float>() / (static_cast<float>(circle_slice_edges_count));
	float const d_spread = spread_length / (static_cast<float>(spread_slice_edges_count));

	// generate vertices iteratively
	size_t index = 0u;
	float theta = 0.0f;
	for (unsigned int i = 0u; i < circle_slice_vertices_count; ++i) {
		float const cos_theta = std::cos(theta);
		float const sin_theta = std::sin(theta);

		float distance_to_centre = spread_start;
		for (unsigned int j = 0u; j < spread_slice_vertices_count; ++j) {
			// vertex
			vertices[index] = glm::vec3(distance_to_centre * cos_theta,
			                            distance_to_centre * sin_theta,
			                            0.0f);

			// texture coordinates
			texcoords[index] = glm::vec3(static_cast<float>(j) / (static_cast<float>(spread_slice_vertices_count)),
			                             static_cast<float>(i) / (static_cast<float>(circle_slice_vertices_count)),
			                             0.0f);

			// tangent
			auto const t = glm::vec3(cos_theta, sin_theta, 0.0f);
			tangents[index] = t;

			// binormal
			auto const b = glm::vec3(-sin_theta, cos_theta, 0.0f);
			binormals[index] = b;

			// normal
			auto const n = glm::cross(t, b);
			normals[index] = n;

			distance_to_centre += d_spread;
			++index;
		}

		theta += d_theta;
	}

	// create index array
	auto index_sets = std::vector<glm::uvec3>(2u * circle_slice_edges_count * spread_slice_edges_count);

	// generate indices iteratively
	index = 0u;
	for (unsigned int i = 0u; i < circle_slice_edges_count; ++i)
	{
		for (unsigned int j = 0u; j < spread_slice_edges_count; ++j)
		{
			index_sets[index] = glm::uvec3(spread_slice_vertices_count * (i + 0u) + (j + 0u),
			                               spread_slice_vertices_count * (i + 0u) + (j + 1u),
			                               spread_slice_vertices_count * (i + 1u) + (j + 1u));
			++index;

			index_sets[index] = glm::uvec3(spread_slice_vertices_count * (i + 0u) + (j + 0u),
			                               spread_slice_vertices_count * (i + 1u) + (j + 1u),
			                               spread_slice_vertices_count * (i + 1u) + (j + 0u));
			++index;
		}
	}

	bonobo::mesh_data data;
	glGenVertexArrays(1, &data.vao);
	assert(data.vao != 0u);
	glBindVertexArray(data.vao);

	auto const vertices_offset = 0u;
	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
	auto const normals_offset = vertices_size;
	auto const normals_size = static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
	auto const texcoords_offset = normals_offset + normals_size;
	auto const texcoords_size = static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
	auto const tangents_offset = texcoords_offset + texcoords_size;
	auto const tangents_size = static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
	auto const binormals_offset = tangents_offset + tangents_size;
	auto const binormals_size = static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
	auto const bo_size = static_cast<GLsizeiptr>(vertices_size
	                                            +normals_size
	                                            +texcoords_size
	                                            +tangents_size
	                                            +binormals_size
	                                            );
	glGenBuffers(1, &data.bo);
	assert(data.bo != 0u);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size, static_cast<GLvoid const*>(vertices.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

	glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size, static_cast<GLvoid const*>(normals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::normals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(normals_offset));

	glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(texcoords.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));

	glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size, static_cast<GLvoid const*>(tangents.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::tangents));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(tangents_offset));

	glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size, static_cast<GLvoid const*>(binormals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::binormals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(binormals_offset));

	glBindBuffer(GL_ARRAY_BUFFER, 0u);

	data.indices_nb = index_sets.size() * 3u;
	glGenBuffers(1, &data.ibo);
	assert(data.ibo != 0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(index_sets.size() * sizeof(glm::uvec3)), reinterpret_cast<GLvoid const*>(index_sets.data()), GL_STATIC_DRAW);

	glBindVertexArray(0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

	return data;
}
