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
				memcpy(baseColor, outMesh->materials[materialIndex].baseColorFactor, sizeof(vec4));
			}

			// Locate attribute accessors robustly
			cgltf_accessor *normalAccessor = NULL, *uvAccessor = NULL;
			for (cgltf_size a = 0; a < primitive->attributes_count; ++a)
			{
				const cgltf_attribute* attr = &primitive->attributes[a];
				if (attr->type == cgltf_attribute_type_position)
					posAccessor = attr->data;
				else if (attr->type == cgltf_attribute_type_normal)
					normalAccessor = attr->data;
				else if (attr->type == cgltf_attribute_type_texcoord && attr->index == 0)
					uvAccessor = attr->data;
			}

			if (!posAccessor)
				continue;

			// Precompute normal matrix from worldTransform
			mat3 normalMatrix;
			glm_mat4_pick3(worldTransform, normalMatrix);
			glm_mat3_inv(normalMatrix, normalMatrix);
			glm_mat3_transpose(normalMatrix);

			// Extract vertices using cgltf API (handles strides/offsets)
			for (cgltf_size v = 0; v < posAccessor->count; ++v)
			{
				Vertex vert = (Vertex){0};

				// Position
				float p[3] = {0};
				cgltf_accessor_read_float(posAccessor, v, p, 3);
				vec4 pos = {p[0], p[1], p[2], 1.0f};
				vec4 transformed;
				glm_mat4_mulv(worldTransform, pos, transformed);
				glm_vec3_copy(transformed, vert.pos);

				// Normal
				if (normalAccessor)
				{
					float n[3] = {0};
					cgltf_accessor_read_float(normalAccessor, v, n, 3);
					vec3 nn = {n[0], n[1], n[2]};
					glm_mat3_mulv(normalMatrix, nn, vert.normal);
					glm_vec3_normalize(vert.normal);
				}
				else
				{
					glm_vec3_copy((vec3){0, 1, 0}, vert.normal);
				}

				// Texcoord 0 (flip V)
				if (uvAccessor)
				{
					float uv[2] = {0};
					cgltf_accessor_read_float(uvAccessor, v, uv, 2);
					vert.texcoord[0] = uv[0];
					vert.texcoord[1] = 1.0f - uv[1];
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
    printf("GLTF materials_count: %zu\n", data->materials_count);


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
			ourMat->baseColorFactor[0] = 1.0f;
			ourMat->baseColorFactor[1] = 1.0f;
			ourMat->baseColorFactor[2] = 1.0f;
			ourMat->baseColorFactor[3] = 1.0f;
			ourMat->metallicFactor = 1.0f;
			ourMat->roughnessFactor = 1.0f;
			ourMat->emissiveFactor[0] = 0.0f;
			ourMat->emissiveFactor[1] = 0.0f;
			ourMat->emissiveFactor[2] = 0.0f;
			ourMat->hasBaseColorTexture = 0;
			ourMat->hasMetallicRoughnessTexture = 0;
			ourMat->hasEmissiveTexture = 0;
			ourMat->baseColorTexturePath = NULL;
			ourMat->metallicRoughnessTexturePath = NULL;
			ourMat->emissiveTexturePath = NULL;
			ourMat->alphaCutoff = 0.5f; // Default alpha cutoff
			ourMat->alphaMode = 0;      // Default to opaque

			// Parse alpha mode and cutoff
			if (mat->alpha_mode == cgltf_alpha_mode_opaque)
			{
				ourMat->alphaMode = 0;
			}
			else if (mat->alpha_mode == cgltf_alpha_mode_mask)
			{
				ourMat->alphaMode = 1;
				ourMat->alphaCutoff = mat->alpha_cutoff;
			}
			else if (mat->alpha_mode == cgltf_alpha_mode_blend)
			{
				ourMat->alphaMode = 2;
			}

			// Parse PBR metallic-roughness properties
			if (mat->has_pbr_metallic_roughness)
			{
				cgltf_pbr_metallic_roughness* pbr = &mat->pbr_metallic_roughness;

				memcpy(ourMat->baseColorFactor, pbr->base_color_factor, sizeof(float) * 4);
				ourMat->metallicFactor = pbr->metallic_factor;
				ourMat->roughnessFactor = pbr->roughness_factor;

				if (pbr->base_color_texture.texture && pbr->base_color_texture.texture->image && pbr->base_color_texture.texture->image->uri)
				{
					ourMat->hasBaseColorTexture = 1;
					const char* uri = pbr->base_color_texture.texture->image->uri;
					size_t full_path_len = strlen(dir_path) + strlen(uri) + 1;
					ourMat->baseColorTexturePath = malloc(full_path_len);
					snprintf(ourMat->baseColorTexturePath, full_path_len, "%s%s", dir_path, uri);
				}

					// Double-sided
					ourMat->doubleSided = mat->double_sided ? true : false;

				if (pbr->metallic_roughness_texture.texture && pbr->metallic_roughness_texture.texture->image && pbr->metallic_roughness_texture.texture->image->uri)
				{
					ourMat->hasMetallicRoughnessTexture = 1;
					const char* uri = pbr->metallic_roughness_texture.texture->image->uri;
					size_t full_path_len = strlen(dir_path) + strlen(uri) + 1;
					ourMat->metallicRoughnessTexturePath = malloc(full_path_len);
					snprintf(ourMat->metallicRoughnessTexturePath, full_path_len, "%s%s", dir_path, uri);
				}
			}

			if (mat->emissive_texture.texture && mat->emissive_texture.texture->image && mat->emissive_texture.texture->image->uri)
			{
				ourMat->hasEmissiveTexture = 1;
				const char* uri = mat->emissive_texture.texture->image->uri;
				size_t full_path_len = strlen(dir_path) + strlen(uri) + 1;
				ourMat->emissiveTexturePath = malloc(full_path_len);
				snprintf(ourMat->emissiveTexturePath, full_path_len, "%s%s", dir_path, uri);
			}

			memcpy(ourMat->emissiveFactor, mat->emissive_factor, sizeof(float) * 3);
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
		memcpy(outMesh->base_color, outMesh->materials[0].baseColorFactor, sizeof(vec4));
		outMesh->has_texture = outMesh->materials[0].hasBaseColorTexture;
		if (outMesh->materials[0].baseColorTexturePath)
		{
			outMesh->texture_path = malloc(strlen(outMesh->materials[0].baseColorTexturePath) + 1);
			strcpy(outMesh->texture_path, outMesh->materials[0].baseColorTexturePath);
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

