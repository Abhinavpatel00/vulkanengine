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
    PointLight lights[8];
    DirectionalLight dirLight;
    // Stylized controls (std140: pack as scalars)
    int stylizedMode;            // 0=PBR, 1=Toon
    float toonSteps;             // levels for diffuse
    float toonSpecularStrength;  // specular band strength 0..1
    float pad0;                  // padding
    float toonShadowSoftness;    // smooth band width
    float toonWrap;              // light wrap
    float rimStrength;           // rim intensity
    float rimWidth;              // rim width exponent
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
    vec2 flippedUV = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y); // 👈 Flip Y

    vec3 albedo;
    float metallic;
    float roughness;
    float alpha = 1.0;

    if (material.hasFlags.x == 1) {
        vec4 texColor = texture(baseColorSampler, flippedUV);
        if (texColor.a < 0.1) discard; // early discard for alpha cutout
        vec4 bc = texColor;
        albedo = bc.rgb * material.baseColorFactor.rgb;
        alpha = bc.a * material.baseColorFactor.a;
    } else {
        albedo = material.baseColorFactor.rgb;
        alpha = material.baseColorFactor.a;
    }

    if (material.hasFlags.y == 1) {
        vec4 metallicRoughness = texture(metallicRoughnessSampler, flippedUV);
        metallic = metallicRoughness.b * material.mr_ac_am.x;
        roughness = metallicRoughness.g * material.mr_ac_am.y;
    } else {
        metallic = material.mr_ac_am.x;
        roughness = material.mr_ac_am.y;
    }

    int alphaMode = int(material.mr_ac_am.w);
    float alphaCutoff = material.mr_ac_am.z;
    if (alphaMode == 1 && alpha < alphaCutoff) discard;

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.cameraPos - fragWorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 color = vec3(0.0);
    vec3 ambient = vec3(0.03) * albedo;

    // Directional light only for now
    vec3 L = normalize(-ubo.dirLight.direction.xyz);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    vec3 radiance = ubo.dirLight.color.rgb;

    int mode = ubo.stylizedMode >= 0 ? ubo.stylizedMode : material.hasFlags.w;
    if (mode == 1) {
        // Light wrap to avoid harsh terminator
        float wrapNdotL = clamp((NdotL + ubo.toonWrap) / (1.0 + ubo.toonWrap), 0.0, 1.0);

        // Soft quantization using smoothstep between bands
        float steps = max(1.0, ubo.toonSteps);
        float t = wrapNdotL * steps;
        float base = floor(t) / steps;
        float frac = t - floor(t);
        float softness = clamp(ubo.toonShadowSoftness, 0.0, 1.0);
        float q = mix(base, base + 1.0 / steps, smoothstep(0.5 - softness, 0.5 + softness, frac));
        vec3 diffuse = albedo * q;

        // Specular band with thresholding and control by roughness
        float hdotn = max(dot(H, N), 0.0);
        float specPow = mix(8.0, 128.0, 1.0 - clamp(roughness, 0.0, 1.0));
        float specVal = pow(hdotn, specPow);
        float specBand = smoothstep(0.6, 0.8, specVal) * ubo.toonSpecularStrength;

        // Rim lighting using 1 - N·V
        float rim = 1.0 - max(dot(N, V), 0.0);
        float rimMask = pow(clamp(rim, 0.0, 1.0), max(0.1, ubo.rimWidth));
        vec3 rimColor = albedo * rimMask * ubo.rimStrength;

        color = ambient + (diffuse + specBand) * radiance + rimColor;
    } else {
        // PBR
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        vec3 spec = numerator / denominator;
        vec3 Lo = (kD * albedo / PI + spec) * radiance * NdotL;
        color = ambient + Lo;
    }

    if (material.hasFlags.z == 1) {
        color += texture(emissiveSampler, flippedUV).rgb * material.emissiveFactor.rgb;
    } else {
        color += material.emissiveFactor.rgb;
    }

    // Apply gamma correction for display
    vec3 gammaColor = pow(clamp(color, 0.0, 1.0), vec3(1.0/2.2));
    outColor = vec4(gammaColor, alpha);
}
