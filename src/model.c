#include "main.h"

#include "main.h"
void ProcessGltfNode(cgltf_node* node, Mesh* outMesh, cgltf_data* data, mat4 parentTransform, Application* app, uint32_t* vertexOffset, uint32_t* indexOffset, uint32_t* primitiveIndex)
{
	mat4 localTransform;
	glm_mat4_identity(localTransform);
	cgltf_node_transform_local(node, (cgltf_float*)localTransform);

	mat4 worldTransform;
	glm_mat4_mul(parentTransform, localTransform, worldTransform);

	if (node->mesh)
	{
		for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i)
		{
			cgltf_primitive* primitive = &node->mesh->primitives[i];
			uint32_t startVertex = *vertexOffset;
			uint32_t startIndex = *indexOffset;
			cgltf_accessor* posAccessor = NULL;

			// Find material index
			int materialIndex = -1;
			if (primitive->material)
			{
				materialIndex = (int)(primitive->material - data->materials);
			}

			// Get base color from material
			vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f}; // default white
			if (materialIndex >= 0 && materialIndex < (int)outMesh->material_count)
			{
				memcpy(baseColor, outMesh->materials[materialIndex].base_color, sizeof(vec4));
			}

			// Find POSITION accessor
			for (cgltf_size a = 0; a < primitive->attributes_count; ++a)
			{
				if (strcmp(primitive->attributes[a].name, "POSITION") == 0)
				{
					posAccessor = primitive->attributes[a].data;
					break;
				}
			}

			if (!posAccessor)
				continue;

			// Extract vertices
			for (cgltf_size v = 0; v < posAccessor->count; ++v)
			{
				Vertex vert = {0};

				for (cgltf_size a = 0; a < primitive->attributes_count; ++a)
				{
					cgltf_attribute* attr = &primitive->attributes[a];
					float* buffer = (float*)attr->data->buffer_view->buffer->data +
					                attr->data->buffer_view->offset / sizeof(float) +
					                attr->data->offset / sizeof(float);

					if (strcmp(attr->name, "POSITION") == 0)
					{
						vec4 pos = {buffer[v * 3], buffer[v * 3 + 1], buffer[v * 3 + 2], 1.0f};
						vec4 transformed;
						glm_mat4_mulv(worldTransform, pos, transformed);
						glm_vec3_copy(transformed, vert.pos);
					}
					else if (strcmp(attr->name, "NORMAL") == 0)
					{
						memcpy(&vert.normal, buffer + v * 3, sizeof(vec3));
					}
					else if (strcmp(attr->name, "TEXCOORD_0") == 0)
					{
						memcpy(&vert.texcoord, buffer + v * 2, sizeof(vec2));
					}
				}

				// Set vertex color from material
				memcpy(vert.color, baseColor, sizeof(vec4));

				outMesh->vertices[(*vertexOffset)++] = vert;
			}

			// Add indices and create primitive entry
			uint32_t indexCount = 0;
			if (primitive->indices)
			{
				indexCount = primitive->indices->count;
				for (cgltf_size k = 0; k < primitive->indices->count; ++k)
				{
					outMesh->indices[(*indexOffset)++] = startVertex + cgltf_accessor_read_index(primitive->indices, k);
				}
			}
			else
			{
				indexCount = posAccessor->count;
				for (cgltf_size k = 0; k < posAccessor->count; ++k)
				{
					outMesh->indices[(*indexOffset)++] = startVertex + k;
				}
			}

			// Create primitive entry
			if (*primitiveIndex < outMesh->primitive_count)
			{
				outMesh->primitives[*primitiveIndex].first_index = startIndex;
				outMesh->primitives[*primitiveIndex].index_count = indexCount;
				outMesh->primitives[*primitiveIndex].material_index = materialIndex;
				(*primitiveIndex)++;
			}
		}
	}

	// Recurse through children
	for (cgltf_size i = 0; i < node->children_count; ++i)
	{
		ProcessGltfNode(node->children[i], outMesh, data, worldTransform, app, vertexOffset, indexOffset, primitiveIndex);
	}
}

void checkMaterials(cgltf_data* data)
{
	printf("Total materials defined in file: %zu\n", data->materials_count);

	size_t usedMaterialCount = 0;
	bool usedMaterialFlags[256] = {false}; // assuming max 256 materials

	for (size_t m = 0; m < data->meshes_count; ++m)
	{
		const cgltf_mesh* mesh = &data->meshes[m];

		for (size_t p = 0; p < mesh->primitives_count; ++p)
		{
			const cgltf_primitive* prim = &mesh->primitives[p];
			if (prim->material)
			{
				size_t materialIndex = prim->material - data->materials;

				if (!usedMaterialFlags[materialIndex])
				{
					usedMaterialFlags[materialIndex] = true;
					usedMaterialCount++;

					printf("Used material %zu: %s\n",
					    materialIndex,
					    prim->material->name ? prim->material->name : "(unnamed)");

					// Print base color factor
					const float* color = prim->material->pbr_metallic_roughness.base_color_factor;
					printf("  Base Color: (%.2f, %.2f, %.2f, %.2f)\n", color[0], color[1], color[2], color[3]);
				}
			}
			else
			{
				printf("Primitive has no material assigned.\n");
			}
		}
	}

	printf("Total materials actually used in primitives: %zu\n", usedMaterialCount);
}

// --- Model Loading ---
// will it be better to load openusd or work on our format for loading data ,we need to be thinking about what data format to ship on production
// it could be that simple json file can be used to extract needed  data from gltf to our simple json format this is not very important to change now we can also
// just inially ship with gltf then research about it later as pixar
void loadGltfModel(const char* path, Mesh* outMesh)
{
	cgltf_options options = {0};
	cgltf_data* data = NULL;

	cgltf_parse_file(&options, path, &data);

	char* dir_path = NULL;
	char* last_slash = strrchr(path, '/');
	if (last_slash)
	{
		size_t dir_len = last_slash - path + 1;
		dir_path = malloc(dir_len + 1);
		strncpy(dir_path, path, dir_len);
		dir_path[dir_len] = '\0';
	}
	else
	{
		dir_path = malloc(3);
		strcpy(dir_path, "./");
	}

	cgltf_load_buffers(&options, data, dir_path);

	// Initialize mesh
	memset(outMesh, 0, sizeof(Mesh));

	// Parse materials first
	outMesh->material_count = data->materials_count;
	if (outMesh->material_count > 0)
	{
		outMesh->materials = calloc(outMesh->material_count, sizeof(Material));

		for (cgltf_size i = 0; i < data->materials_count; ++i)
		{
			cgltf_material* mat = &data->materials[i];
			Material* ourMat = &outMesh->materials[i];

			// Initialize with default values
			ourMat->base_color[0] = 1.0f;
			ourMat->base_color[1] = 1.0f;
			ourMat->base_color[2] = 1.0f;
			ourMat->base_color[3] = 1.0f;
			ourMat->has_texture = 0;
			ourMat->texture_index = -1;
			ourMat->texture_path = NULL;
			ourMat->alpha_cutoff = 0.5f; // Default alpha cutoff
			ourMat->alpha_mode = 0;      // Default to opaque

			// Parse alpha mode and cutoff
			if (mat->alpha_mode == cgltf_alpha_mode_opaque)
			{
				ourMat->alpha_mode = 0;
			}
			else if (mat->alpha_mode == cgltf_alpha_mode_mask)
			{
				ourMat->alpha_mode = 1;
				ourMat->alpha_cutoff = mat->alpha_cutoff;
			}
			else if (mat->alpha_mode == cgltf_alpha_mode_blend)
			{
				ourMat->alpha_mode = 2;
			}

			if (mat->has_pbr_metallic_roughness)
			{
				cgltf_pbr_metallic_roughness* pbr = &mat->pbr_metallic_roughness;
				memcpy(ourMat->base_color, pbr->base_color_factor, sizeof(float) * 4);

				if (pbr->base_color_texture.texture &&
				    pbr->base_color_texture.texture->image &&
				    pbr->base_color_texture.texture->image->uri)
				{

					const char* uri = pbr->base_color_texture.texture->image->uri;
					size_t full_path_len = strlen(dir_path) + strlen(uri) + 1;
					ourMat->texture_path = malloc(full_path_len);
					snprintf(ourMat->texture_path, full_path_len, "%s%s", dir_path, uri);
					ourMat->has_texture = 1;
				}
			}
		}
	}

	// Count vertices, indices, and primitives
	size_t totalVertices = 0, totalIndices = 0, totalPrimitives = 0;
	for (cgltf_size i = 0; i < data->meshes_count; ++i)
	{
		cgltf_mesh* mesh = &data->meshes[i];
		for (cgltf_size j = 0; j < mesh->primitives_count; ++j)
		{
			cgltf_primitive* prim = &mesh->primitives[j];
			if (prim->attributes_count == 0)
				continue;

			totalVertices += prim->attributes[0].data->count;
			totalIndices += prim->indices ? prim->indices->count : prim->attributes[0].data->count;
			totalPrimitives++;
		}
	}

	// Allocate arrays
	outMesh->vertex_count = totalVertices;
	outMesh->index_count = totalIndices;
	outMesh->primitive_count = totalPrimitives;
	outMesh->vertices = calloc(totalVertices, sizeof(Vertex));
	outMesh->indices = malloc(totalIndices * sizeof(u32));
	outMesh->primitives = calloc(totalPrimitives, sizeof(Primitive));

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t primitiveIndex = 0;
	mat4 identity;
	glm_mat4_identity(identity);

	for (cgltf_size i = 0; i < data->scenes[0].nodes_count; ++i)
	{
		ProcessGltfNode(data->scenes[0].nodes[i], outMesh, data, identity, NULL, &vertexOffset, &indexOffset, &primitiveIndex);
	}

	// Legacy support - use first material for backward compatibility
	if (outMesh->material_count > 0)
	{
		memcpy(outMesh->base_color, outMesh->materials[0].base_color, sizeof(vec4));
		outMesh->has_texture = outMesh->materials[0].has_texture;
		if (outMesh->materials[0].texture_path)
		{
			outMesh->texture_path = malloc(strlen(outMesh->materials[0].texture_path) + 1);
			strcpy(outMesh->texture_path, outMesh->materials[0].texture_path);
		}
	}
	else
	{
		// Fallback
		outMesh->texture_path = malloc(strlen("Bark_DeadTree.png") + 1);
		strcpy(outMesh->texture_path, "Bark_DeadTree.png");
	}

	free(dir_path);
	cgltf_free(data);
}

