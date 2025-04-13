#ifdef GL_ES
precision mediump float;
varying vec2 v_texcoord0;
#else
in vec2 v_texcoord0;
#endif

#include <bgfx_shader.sh>

uniform sampler2D u_diffuseTex; // Diffuse texture for the comic element.
uniform vec4 u_comicColor;      // User-controlled color.

void main()
{
    vec4 texColor = texture2D(u_diffuseTex, v_texcoord0);
    // Multiply the sampled texture color with the comic color.
    gl_FragColor = texColor * u_comicColor;
}
