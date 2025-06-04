#ifdef GL_ES
precision mediump float;
varying vec3 v_normal;
varying vec3 v_pos;
#else
in vec3 v_normal;
in vec3 v_pos;
#endif

#include <bgfx_shader.sh>

// The uniforms below must be set exactly as in your scene‐render pass:
uniform vec4  u_inkColor;    // RGB = ink color, alpha ignored
uniform vec4  u_params;      // (unused, strokeMult = u_params.y; angle1 = u_params.z)
uniform vec4 u_paramsLayer;
uniform vec4 u_extraParams;
uniform sampler2D u_noiseTex; // noise texture for generating 3D hatches

// ------------------------------------------------------------------------
// Re-declare your scene’s crosshatch helper functions exactly here.
// ------------------------------------------------------------------------

float luma(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float aastep(float threshold, float value)
{
#ifdef GL_OES_standard_derivatives
    float afwidth = length(vec2(dFdx(value), dFdy(value))) * 0.70710678;
    return smoothstep(threshold - afwidth, threshold + afwidth, value);
#else
    return step(threshold, value);
#endif
}

float texh(in vec2 p, in float str)
{
    float rz = 1.0;
    for (int i = 0; i < 10; i++)
    {
        float g = texture2D(u_noiseTex, vec2(0.025, 0.5) * p).r;
        g = smoothstep(0.0 - str * 0.1, 2.3 - str * 0.1, g);
        rz = min(1.0 - g, rz);
        p = p.yx;
        p += 0.7;
        p *= 1.52;
        if (float(i) > str) break;
    }
    return rz * 1.05;
}

float texcube(in vec3 p, in vec3 n, in float str, float a)
{
    float s = sin(a), c = cos(a);
    mat2 rot = mat2(c, -s, s, c);
    vec3 v;
    v.x = texh(rot * p.yz, str);
    v.y = texh(rot * p.zx, str);
    v.z = texh(rot * p.xy, str);
    return dot(v, n * n);
}

// In native shadow sampler mode the depth is written automatically
// so this shader can output a constant color.
void main()
{
    // 1) World normal:
    vec3 N = normalize(v_normal);

    float layerPatternScale = u_paramsLayer.x;
    float layerStrokeMult = u_paramsLayer.y;
    float layerAngle = u_paramsLayer.z;

    // 4) Build scaled position for the hatch pattern:
    vec3 p_scaled2 = v_pos * u_extraParams.x; // u_extraParams.x = patternScale

    // 6) Sample the 3D hatch function:
    float line2 = texcube(p_scaled2, N, layerStrokeMult * u_paramsLayer.w, layerAngle);

    // 7) Smoothstep to get r in [0,1]:
    float r2 = 1.0 - step(0.5, line2);

    // 8) Mix black (background) with ink lines (u_inkColor.rgb):
    vec3 hatchColor = mix(vec3(1.0), u_inkColor.rgb, r2);

    // 9) Output final RGBA (fully opaque):
    gl_FragColor = vec4(hatchColor, 1.0);

    //gl_FragColor = vec4(0.0);
}
