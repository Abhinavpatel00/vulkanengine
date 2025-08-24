#include "main.h"

void materials_build_gpu_ubos(Application* app)
{
    if (!app->mesh.material_count) return;
    if (app->materialUniformBuffers) {
        materials_free_gpu_ubos(app);
    }
    app->materialUniformBuffers = calloc(app->mesh.material_count, sizeof(Buffer));

    for (u32 i = 0; i < app->mesh.material_count; ++i) {
        Material* src = &app->mesh.materials[i];
        MaterialGPU gpu = {0};
        glm_vec4_copy(src->baseColorFactor, gpu.baseColorFactor);
        gpu.emissiveFactor[0] = src->emissiveFactor[0];
        gpu.emissiveFactor[1] = src->emissiveFactor[1];
        gpu.emissiveFactor[2] = src->emissiveFactor[2];
        gpu.mr_ac_am[0] = src->metallicFactor;
        gpu.mr_ac_am[1] = src->roughnessFactor;
        gpu.mr_ac_am[2] = src->alphaCutoff;
        gpu.mr_ac_am[3] = (float)src->alphaMode;
        gpu.hasFlags[0] = src->hasBaseColorTexture;
        gpu.hasFlags[1] = src->hasMetallicRoughnessTexture;
        gpu.hasFlags[2] = src->hasEmissiveTexture;
        createBuffer(app, &app->materialUniformBuffers[i], sizeof(MaterialGPU), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        memcpy(app->materialUniformBuffers[i].data, &gpu, sizeof(MaterialGPU));
    }
}

void materials_free_gpu_ubos(Application* app)
{
    if (!app->materialUniformBuffers) return;
    for (u32 i = 0; i < app->mesh.material_count; ++i) {
        destroyBuffer(app->device, &app->materialUniformBuffers[i]);
    }
    free(app->materialUniformBuffers);
    app->materialUniformBuffers = NULL;
}
