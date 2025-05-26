#include <bgfx_shader.sh>

#ifdef GL_ES
precision mediump float;
#endif

// Inputs from vertex shader
varying vec3 v_normal;
varying vec3 v_pos;
varying vec2 v_texcoord0;
varying vec3 v_viewDir;
varying vec3 v_lightDirs[16];
varying float v_lightDistances[16];

// ----- Lighting uniforms -----
uniform vec4 u_lights[64];
uniform vec4 u_numLights; // x component holds the number of lights.

// ----- Uniforms for crosshatching effect -----
uniform vec4 u_tint;
uniform vec4 u_inkColor;    // Ink color for crosshatching
uniform vec4 u_cameraPos;   // Camera position for hatch adjustments
uniform vec4 u_e;           // Epsilon value in u_e.x (for smoothstep)
uniform sampler2D u_noiseTex; // Noise texture for crosshatching

// ----- Uniform for cross-hatch parameters -----
uniform vec4 u_params;      // y = stroke multiplier, z = first angle, w = second angle

// ----- Extra parameters as a vec4 -----
uniform vec4 u_extraParams; // x = pattern scale, y = line thickness, z = transparency, w = mode

// ----- Diffuse texture -----
uniform sampler2D u_diffuseTex;

// ----- Uniform for the object's override color -----
uniform vec4 u_objectColor;

// ----- Extra parameters for 2nd Layer as a vec4 -----
uniform vec4 u_paramsLayer; // x = pattern scale, y = stroke, z = angle, w = line thickness

// NEW uniforms for tiling/offset and albedo factor:
uniform vec4 u_uvTransform;   // (tilingU, tilingV, offsetU, offsetV)
uniform vec4 u_albedoFactor;  // (r, g, b, a) color tint

// ----- Helper functions for crosshatching -----
float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 blendDarken(vec3 base, vec3 ink, float opacity) {
    return mix(base, min(base, ink), opacity);
}

float aastep(float threshold, float value) {
#ifdef GL_OES_standard_derivatives
    float afwidth = length(vec2(dFdx(value), dFdy(value))) * 0.70710678;
    return smoothstep(threshold - afwidth, threshold + afwidth, value);
#else
    return step(threshold, value);
#endif  
}

float texh(in vec2 p, in float str) {
    float rz = 1.0;
    for (int i = 0; i < 10; i++) {
        float g = texture2D(u_noiseTex, vec2(0.025, 0.5) * p).r;
        g = smoothstep(0.0 - str * 0.1, 2.3 - str * 0.1, g);
        rz = min(1.0 - g, rz);
        p = p.yx;
        p += 0.7;
        p *= 1.52;
        if (float(i) > str)
            break;
    }
    return rz * 1.05;
}

float texcube(in vec3 p, in vec3 n, in float str, float a) {
    float s = sin(a);
    float c = cos(a);
    mat2 rot = mat2(c, -s, s, c);
    vec3 v;
    v.x = texh(rot * p.yz, str);
    v.y = texh(rot * p.zx, str);
    v.z = texh(rot * p.xy, str);
    return dot(v, n * n);
}

void main() {
    // Use the normalized normal from vertex shader
    vec3 N = normalize(v_normal);
    
    // --- Lighting Calculation (using pre-calculated light directions) ---
    vec3 lighting = vec3(0.0);
    int numLights = int(u_numLights.x);
    
    for (int i = 0; i < numLights; i++) {
        int offset = i * 4;
        float lightType = u_lights[offset].x;     // 0: directional, 1: point, 2: spot
        float intensity = u_lights[offset].y;     // Light intensity
        vec3 lightDir = normalize(u_lights[offset+2].xyz); // For spot lights
        float coneAngle = u_lights[offset+2].w;   // For spotlights
        vec3 lightColor = u_lights[offset+3].rgb; // Light color
        float range = u_lights[offset+3].w;       // For attenuation
        
        // Get pre-calculated light direction and distance
        vec3 L = v_lightDirs[i];
        float diff = max(dot(N, L), 0.0);
        
        // For point and spot lights, apply attenuation
        if (lightType != 0.0) {
            float distance = v_lightDistances[i];
            float attenuation = clamp(1.0 - distance / range, 0.0, 1.0);
            diff *= attenuation;
        }
        
        // Spotlight calculation
        if (lightType == 2.0) {
            float theta = dot(normalize(-L), lightDir);
            float cutoff = cos(coneAngle);
            if (theta < cutoff) {
                diff = 0.0;
            } else {
                diff *= smoothstep(cutoff, 1.0, theta);
            }
        }
        
        lighting += lightColor * intensity * diff;
    }
    
    // --- Texture/Material ---
    // Apply tiling & offset
    vec2 uvScaled = v_texcoord0 * u_uvTransform.xy + u_uvTransform.zw;
    
    // Sample the diffuse texture
    vec4 texSample = texture2D(u_diffuseTex, uvScaled);
    
    // Apply the color tint
    vec3 tintedBase = texSample.rgb * u_albedoFactor.rgb;
    
    // Apply lighting
    vec3 litColor = tintedBase * lighting;
    
    // --- Crosshatch Effect Selection ---
    int mode = int(u_extraParams.w);
    vec3 crossColor;
    
    // Apply the selected crosshatching mode
    if (mode == 0) {
        // --- Original Crosshatch Effect ---
        float lumVal = luma(litColor);
        float lVal = 1.0 - lumVal;
        float darks = 1.0 - 2.0 * lumVal;
        
        float strokeMult = u_params.y;
        float angle1 = u_params.z;
        float angle2 = u_params.w;
        
        vec3 p_scaled = v_pos * u_extraParams.x;
        
        float line = texcube(p_scaled, N, lVal * strokeMult * u_extraParams.y, angle1);
        float lineDark = texcube(p_scaled, N, darks * strokeMult * u_extraParams.y, angle2);
        
        float epsilon = u_e.x;
        float r = 1.0 - smoothstep(lVal - epsilon, lVal + epsilon, line);
        float rDark = 1.0 - smoothstep(lVal - epsilon, lVal + epsilon, lineDark);
        
        vec3 inkedColor = blendDarken(litColor, u_inkColor.xyz, 0.5 * r);
        crossColor = mix(inkedColor, u_inkColor.xyz, rDark);
    }
    else if (mode == 1) {
        // --- Modified Crosshatch Effect ---
        float lumVal = luma(litColor);
        float lVal = 1.0 - lumVal;
        
        float strokeMult = u_params.y;
        float angle1 = u_params.z;
        
        vec3 p_scaled = v_pos * u_extraParams.x;
        
        float line = texcube(p_scaled, N, lVal * strokeMult * u_extraParams.y, angle1);
        
        float epsilon = u_e.x;
        float r = 1.0 - smoothstep(lVal - epsilon, lVal + epsilon, line);
        
        crossColor = mix(litColor, u_inkColor.xyz, r);
    }
    else if (mode == 2) {
        // --- Distance-based Crosshatch Effect with Two Layers ---
        float dist = length(u_cameraPos.xyz - v_pos);
        float referenceDist = 2.0;
        float factor = referenceDist / max(dist, referenceDist);
        
        // Layer 1
        float finalScale = u_extraParams.x * factor;
        vec3 p_scaled = v_pos * finalScale;
        
        float lumVal = luma(litColor);
        float lVal = 1.0 - lumVal;
        
        float strokeMult = u_params.y;
        float angle1 = u_params.z;
        
        float line = texcube(p_scaled, N, lVal * strokeMult * u_extraParams.y, angle1);
        float epsilon = u_e.x;
        float r = 1.0 - smoothstep(lVal - epsilon, lVal + epsilon, line);
        
        // Layer 2
        float layerPatternScale = u_paramsLayer.x;
        float layerStrokeMult = u_paramsLayer.y;
        float layerAngle = u_paramsLayer.z;
        
        float finalScale2 = layerPatternScale * factor;
        vec3 p_scaled2 = v_pos * finalScale2;
        
        float line2 = texcube(p_scaled2, N, layerStrokeMult * u_paramsLayer.w, layerAngle);
        float r2 = 1.0 - step(0.5, line2);
        
        vec3 crosshatch = mix(litColor, u_inkColor.xyz, r);
        crossColor = mix(crosshatch, u_inkColor.xyz, r2);
    }
    else if (mode == 3) {
        // Simple lighting mode
        crossColor = litColor;
    }
    
    // Blend the crosshatch with the lit color
    vec3 finalColor = mix(litColor, crossColor, u_extraParams.z);
    
    // Apply the object color override
    finalColor *= u_objectColor.rgb;
    
    vec4 finalColor4 = vec4(finalColor, 1.0);
    gl_FragColor = mix(finalColor4, u_tint, u_tint.a);
}