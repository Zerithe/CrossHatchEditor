#ifdef GL_ES
precision mediump float;
attribute vec3 a_position;
varying vec4 v_position; // Pass transformed position to fragment shader.
#else
in vec3 a_position;
out vec4 v_position;
#endif

#include <bgfx_shader.sh>

void main()
{
    vec4 pos = u_viewProj * u_model[0] * vec4(a_position, 1.0);
    gl_Position = pos;
    v_position = pos;
}
