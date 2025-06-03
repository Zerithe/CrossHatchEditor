#ifdef GL_ES
precision mediump float;
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texcoord0;
varying vec3 v_normal;
varying vec3 v_pos;
varying vec2 v_texcoord0;
varying vec4 v_shadowcoord;  // New varying for shadow coordinate
#else
in vec3 a_position;
in vec3 a_normal;
in vec2 a_texcoord0;
out vec3 v_normal;
out vec3 v_pos;
out vec2 v_texcoord0;
out vec4 v_shadowcoord;      // New varying for shadow coordinate
#endif

#include <bgfx_shader.sh>

// Uniform for the light transformation matrix computed in the shadow pass.
uniform mat4 u_lightMtx;

void main()
{
    // Transform the vertex to world space.
    vec4 worldPos = u_model[0] * vec4(a_position, 1.0);
    v_pos = worldPos.xyz;
    
    // Transform the normal (ignoring translation) and normalize.
    v_normal = normalize((u_model[0] * vec4(a_normal, 0.0)).xyz);
    
    // Pass through texture coordinates.
    v_texcoord0 = a_texcoord0;
    
    // Compute the shadow coordinate.
    // Offset along normal to reduce self-shadowing.
    float shadowMapOffset = 0.001;
    vec3 posOffset = worldPos.xyz + v_normal * shadowMapOffset;
    v_shadowcoord = u_lightMtx * vec4(posOffset, 1.0);
    
    // Final clip-space position.
    gl_Position = u_viewProj * worldPos;
}
