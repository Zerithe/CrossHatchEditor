#ifdef GL_ES
precision mediump float;
#else
#endif

#include <bgfx_shader.sh>

// In native shadow sampler mode the depth is written automatically
// so this shader can output a constant color.
void main()
{
    gl_FragColor = vec4_splat(0.0);
}
