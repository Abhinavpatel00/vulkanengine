#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

struct PointLight {
    vec4 position;
    vec4 color;
};

struct DirectionalLight {
    vec4 direction;
    vec4 color;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    mat4 model;
    vec3 cameraPos;
    uint numLights;
    PointLight lights[4];
    DirectionalLight dirLight;
} ubo;

layout(binding = 1) uniform sampler2D baseColorSampler;
layout(binding = 2) uniform sampler2D metallicRoughnessSampler;
layout(binding = 3) uniform sampler2D emissiveSampler;

layout(binding = 4) uniform MaterialUBO {
    vec4 baseColorFactor;   // rgba
    vec4 emissiveFactor;    // rgb + pad
    vec4 mr_ac_am;          // x: metallic, y: roughness, z: alphaCutoff, w: alphaMode (as float)
    ivec4 hasFlags;         // x: hasBaseColor, y: hasMetallicRoughness, z: hasEmissive, w: unused
} material;

const float PI = 3.14159265359;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 albedo;
    float metallic;
    float roughness;
    float alpha = 1.0;

    if (material.hasFlags.x == 1) {
        vec4 bc = texture(baseColorSampler, fragTexCoord);
        albedo = bc.rgb * material.baseColorFactor.rgb;
        alpha = bc.a * material.baseColorFactor.a;
    } else {
        albedo = material.baseColorFactor.rgb;
        alpha = material.baseColorFactor.a;
    }

    if (material.hasFlags.y == 1) {
        vec4 metallicRoughness = texture(metallicRoughnessSampler, fragTexCoord);
        metallic = metallicRoughness.b * material.mr_ac_am.x;
        roughness = metallicRoughness.g * material.mr_ac_am.y;
    } else {
        metallic = material.mr_ac_am.x;
        roughness = material.mr_ac_am.y;
    }

    int alphaMode = int(material.mr_ac_am.w);
    float alphaCutoff = material.mr_ac_am.z;
    if (alphaMode == 1 && alpha < alphaCutoff) discard; // mask

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.cameraPos - fragWorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // Directional light
    vec3 L = normalize(-ubo.dirLight.direction.xyz);
    vec3 H = normalize(V + L);
    float distance = 1.0; // Directional light is infinitely far
    float attenuation = 1.0;
    vec3 radiance = ubo.dirLight.color.rgb;

    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 spec = numerator / denominator;

    Lo += (kD * albedo / PI + spec) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    if (material.hasFlags.z == 1) {
        color += texture(emissiveSampler, fragTexCoord).rgb * material.emissiveFactor.rgb;
    } else {
        color += material.emissiveFactor.rgb;
    }

    outColor = vec4(color, alpha);
}