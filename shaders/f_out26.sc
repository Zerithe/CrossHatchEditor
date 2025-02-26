#ifdef GL_ES
precision mediump float;
varying vec3 v_normal;
varying vec3 v_pos;
varying vec2 v_texcoord0;
#else
in vec3 v_normal;
in vec3 v_pos;
in vec2 v_texcoord0;
#endif

#include <bgfx_shader.sh>

// ----- Lighting uniforms -----
// Each light is packed into 4 vec4’s. For example, with MAX_LIGHTS=16, you’ll have 64 vec4’s.
uniform vec4 u_lights[64];
uniform vec4 u_numLights; // x component holds the number of lights.

// ----- Uniforms for crosshatching effect -----
uniform vec4 u_tint;
uniform vec4 u_inkColor;    // Ink color for crosshatching
uniform vec4 u_cameraPos;   // Camera position (if needed for hatch calculations)
uniform vec4 u_e;           // Epsilon value in u_e.x (for smoothstep)
uniform sampler2D u_noiseTex; // Noise texture for crosshatching

// ----- Uniform for cross-hatch parameters -----
// u_params.y = stroke multiplier (default 5.0)
// u_params.z = first hatch angle factor (default TAU/8)
// u_params.w = second hatch angle factor (default TAU/16)
uniform vec4 u_params;

// ----- Extra parameters as a vec4 -----
// u_extraParams.x = pattern scale
// u_extraParams.y = line thickness
uniform vec4 u_extraParams;

// ----- Diffuse texture -----
uniform sampler2D u_diffuseTex;

// ----- Uniform for the object’s override color -----
uniform vec4 u_objectColor;

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

void main()
{
    // --- Lighting Calculation ---
    vec3 N = normalize(v_normal);
    vec3 baseColor = texture2D(u_diffuseTex, v_texcoord0).rgb;
    
    vec3 lighting = vec3(0.0);
    int numLights = int(u_numLights.x);
    for (int i = 0; i < numLights; i++) {
        int offset = i * 4;
        float lightType = u_lights[offset].x; // 0: directional, 1: point, 2: spot.
        float intensity = u_lights[offset].y;
        vec3 lightPos = u_lights[offset+1].xyz;
        vec3 lightDir = normalize(u_lights[offset+2].xyz);
        float coneAngle = u_lights[offset+2].w; // For spotlights if needed
        vec3 lightColor = u_lights[offset+3].rgb;
        float range = u_lights[offset+3].w; // For attenuation if needed
        
        vec3 L;
        if (lightType == 0.0) { // directional
            L = -lightDir;
        } else {
            L = normalize(lightPos - v_pos);
        }
        float diff = max(dot(N, L), 0.0);

        // For point and spot lights, apply attenuation.
        if (lightType != 0.0) {
            float distance = length(lightPos - v_pos);
            // Simple linear attenuation (clamped)
            float attenuation = clamp(1.0 - distance / range, 0.0, 1.0);
            diff *= attenuation;
        }

        // If this is a spot light, apply a cutoff and smooth falloff.
        if (lightType == 2.0) {
            // Compute the angle between the light's direction and the direction from light to fragment.
            // Here, lightDir should be the direction the spotlight is facing.
            float theta = dot(normalize(-L), lightDir);
            float cutoff = cos(coneAngle);
            if (theta < cutoff) {
                diff = 0.0;
            } else {
                // Optionally smooth the edge of the spotlight.
                //float epsilon = 0.1; // adjust as needed for softness
                //diff *= smoothstep(cutoff, cutoff + epsilon, theta);
                diff *= smoothstep(cutoff, 1.0, theta);
            }
        }

        lighting += lightColor * intensity * diff;
    }
    vec3 litColor = baseColor * lighting;
    
    // --- Crosshatch Effect ---
    float lumVal = luma(litColor);
    float lVal = 1.0 - lumVal;
    float darks = 1.0 - 2.0 * lumVal;

    // Use the stroke multiplier and angle factors from u_params:
    float strokeMult = u_params.y;
    float angle1 = u_params.z;
    float angle2 = u_params.w;

    // Scale the world position by the pattern scale (u_extraParams.x)
    vec3 p_scaled = v_pos * u_extraParams.x;

    // Multiply the stroke multiplier by the line thickness factor (u_extraParams.y)
    float line = texcube(p_scaled, N, lVal * strokeMult * u_extraParams.y, angle1);
    float lineDark = texcube(p_scaled, N, darks * strokeMult * u_extraParams.y, angle2);
    
    float epsilon = u_e.x;
    float r = 1.0 - smoothstep(lVal - epsilon, lVal + epsilon, line);
    float rDark = 1.0 - smoothstep(lVal - epsilon, lVal + epsilon, lineDark);
    
    //mix(litColor, u_inkColor.xyz, r);
    vec3 inkedColor = blendDarken(litColor, u_inkColor.xyz, 0.5 * r);
    vec3 crossColor = mix(inkedColor, u_inkColor.xyz, rDark);
    
    // Blend the crosshatch with the lit color (adjust blend factor as desired)
    vec3 finalColor = mix(litColor, crossColor, u_extraParams.z);
    
    // --- Apply the object color override:
    finalColor *= u_objectColor.rgb;
    if (u_tint == vec4(0.3, 0.3, 2.0, 1.0)) {
    	gl_FragColor = u_tint; // Override with exact tint
    } 
    else {
    	gl_FragColor = vec4(finalColor, 1.0);
    }
}
