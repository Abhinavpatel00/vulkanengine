#version 450

#define MAX_POINT_LIGHTS 8

struct PointLight {
    vec3 position;
    float _pad1;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float _pad2;
};

struct DirectionalLight {
    vec3 direction;
    float _pad1;
    vec3 color;
    float intensity;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    mat4 model;
    vec3 cameraPos;
    uint numLights;
    PointLight lights[MAX_POINT_LIGHTS];
    DirectionalLight dirLight;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 3) uniform BaseColorUBO_t { vec4 baseColor; } baseColorUbo;
layout(binding = 4) uniform HasTextureUBO_t { int hasTexture; } hasTextureUbo;
layout(binding = 5) uniform AlphaCutoffUBO_t { float alphaCutoff; } alphaCutoffUbo;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragColor; // ✅ RECEIVE FROM VERTEX

layout(location = 0) out vec4 outColor;

vec3 calculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    vec3 ambient = 0.1 * light.color * light.intensity;
    vec3 diffuse = diff * light.color * light.intensity;
    vec3 specular = spec * light.color * light.intensity * 0.5;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse) * albedo + specular;
}

vec3 calculateDirLight(DirectionalLight light, vec3 normal, vec3 viewDir, vec3 albedo) {
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    vec3 ambient = 0.1 * light.color * light.intensity;
    vec3 diffuse = diff * light.color * light.intensity;
    vec3 specular = spec * light.color * light.intensity * 0.5;

    return (ambient + diffuse) * albedo + specular;
}

void main() {
    vec4 texColor = vec4(1.0);
    vec3 albedo;
    float alpha = 1.0;
    
    if (hasTextureUbo.hasTexture == 1) {
        texColor = texture(texSampler, fragTexCoord);
        albedo = texColor.rgb * fragColor.rgb; // ✅ Use vertex color
        alpha = texColor.a * fragColor.a;
    } else {
        albedo = fragColor.rgb; // ✅ Use vertex color
        alpha = fragColor.a;
    }
    
    // Alpha cutoff test
    if (alphaCutoffUbo.alphaCutoff > 0.0 && alpha < alphaCutoffUbo.alphaCutoff) {
        discard;
    }

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(ubo.cameraPos - fragWorldPos);

    vec3 result = vec3(0.0);
    for (int i = 0; i < int(ubo.numLights); ++i) {
        if (i >= MAX_POINT_LIGHTS) break;
        result += calculatePointLight(ubo.lights[i], normal, fragWorldPos, viewDir, albedo);
    }

    result += calculateDirLight(ubo.dirLight, normal, viewDir, albedo);

    result += 0.15 * albedo;

    outColor = vec4(result, alpha);
}
