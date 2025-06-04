#ifdef GL_ES
precision mediump float;
attribute vec3 a_position;
attribute vec3 a_normal;
varying vec3 v_normal;
varying vec3 v_pos;
#else
in vec3 a_position;
in vec3 a_normal;
out vec3 v_normal;
out vec3 v_pos;
#endif

#include <bgfx_shader.sh>

void main()
{
    // Transform vertex using model, view, and projection matrices.
    //gl_Position = u_viewProj * u_model[0] * vec4(a_position, 1.0);
    // World‐space position:
    vec4 worldPos = u_model[0] * vec4(a_position, 1.0);
    v_pos = worldPos.xyz;

    // World‐space normal:
    v_normal = normalize((u_model[0] * vec4(a_normal, 0.0)).xyz);

    // Project into shadow‐map (or camera):
    //gl_Position = u_modelViewProj * vec4(a_position, 1.0);

    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );
}
