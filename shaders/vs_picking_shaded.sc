#ifdef GL_ES
precision mediump float;
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec4 a_color;
attribute vec2 a_texcoord0;
varying vec4 v_color;
#else
in vec3 a_position;
in vec3 a_normal;
in vec4 a_color;
in vec2 a_texcoord0;
out vec4 v_color;
#endif

#include <bgfx_shader.sh>

void main()
{
    // Compute world-space position and then transform to clip space.
    gl_Position = u_viewProj * u_model[0] * vec4(a_position, 1.0);
    // Pass the vertex color to the fragment shader (if needed for shading).
    v_color = a_color;
}