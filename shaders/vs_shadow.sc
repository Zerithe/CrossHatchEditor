#ifdef GL_ES
precision mediump float;
attribute vec3 a_position;
#else
in vec3 a_position;
#endif

#include <bgfx_shader.sh>

void main()
{
    // Transform vertex using model, view, and projection matrices.
    gl_Position = u_viewProj * u_model[0] * vec4(a_position, 1.0);
}
