$input v_normal, v_pos

#include <bgfx_shader.sh>

uniform vec4 u_lightDir;        // Light direction
uniform vec4 u_lightColor;      // Light color
uniform sampler2D u_hatchTex;   // Hatching texture
uniform vec3 u_inkColor;
//fix scale and epsilon for shaders to display lines correctly
uniform float u_scale;
uniform float u_epsilon;

#define TAU 6.28318530718

// Procedural replacement for noise function
float proceduralNoise(in vec2 x) {
    return fract(sin(dot(x, vec2(12.9898, 78.233))) * 43758.5453);
}

float texh(in vec2 p, in float str) {
    float rz = 1.0;
    for (int i = 0; i < 10; i++) {
        float g = proceduralNoise(vec2(0.025, 0.5) * p);
        g = smoothstep(0.0 - str * 0.1, 2.3 - str * 0.1, g);
        rz = min(1.0 - g, rz);
        p.xy = p.yx;
        p += 0.7;
        p *= 1.52;
        if (float(i) > str) break;
    }
    return rz * 1.05;
}

float texcube(in vec3 p, in vec3 n, in float str, float a) {
    float s = sin(a);
    float c = cos(a);
    mat2 rot = mat2(c, -s, s, c);
    vec3 v = vec3(texh(rot * p.yz, str), texh(rot * p.zx, str), texh(rot * p.xy, str));
    return dot(v, n * n);
}

float blendDarken(float base, float blend) {
    return min(blend, base);
}

vec3 blendDarken(vec3 base, vec3 blend) {
    return vec3(blendDarken(base.r, blend.r), blendDarken(base.g, blend.g), blendDarken(base.b, blend.b));
}

vec3 blendDarken(vec3 base, vec3 blend, float opacity) {
    return (blendDarken(base, blend) * opacity + base * (1.0 - opacity));
}

void main() {
    vec3 lightDir = normalize(u_lightDir.xyz);
    vec3 normal = normalize(v_normal);

    // Calculate diffuse lighting
    float intensity = max(dot(normal, lightDir), 0.0);
    float crossHatchIntensity = 0.0;

    // Quantize intensity to create cel shading effect
    if (intensity > 0.95) {
        intensity = 1.0;
        crossHatchIntensity = 1.0;
    } else if (intensity > 0.5) {
        intensity = 0.6;
        crossHatchIntensity = 0.6;
    } else if (intensity > 0.25) {
        intensity = 0.4;
        crossHatchIntensity = 0.4;
    } else {
        intensity = 0.2;
        crossHatchIntensity = 0.2;
    }

    vec3 coords = v_pos.xyz * u_scale;
    vec3 eye = normalize(-v_pos);
    vec3 ref = reflect(normal, eye);

    float l = intensity; // Replace paper texture intensity with a constant
    float darks = 1.0 - 2.0 * l;

    float line = texcube(coords + 0.0 * ref, normalize(v_normal), l * 5.0, TAU / 8.0);
    float lineDark = texcube(coords + 0.0 * ref, normalize(v_normal), darks * 5.0, TAU / 16.0);

    float r = 1.0 - smoothstep(l - u_epsilon, l + u_epsilon, line);
    float rDark = 1.0 - smoothstep(l - u_epsilon, l + u_epsilon, lineDark);

    vec3 finalColor = blendDarken(u_lightColor.rgb * intensity, u_inkColor, 0.5 * r);
    finalColor = blendDarken(finalColor, u_inkColor, 0.5 * rDark);

    gl_FragColor = vec4(finalColor, 1.0);
}