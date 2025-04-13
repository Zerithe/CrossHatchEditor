#ifdef GL_ES
precision mediump float;
attribute vec3 a_position;
attribute vec2 a_texcoord0;
varying vec2 v_texcoord0;
#else
in vec3 a_position;
in vec2 a_texcoord0;
out vec2 v_texcoord0;
#endif

#include <bgfx_shader.sh>

void main()
{
    v_texcoord0 = a_texcoord0;
    vec4 worldPos = u_model[0] * vec4(a_position, 1.0);
    gl_Position = u_viewProj * worldPos;
}
