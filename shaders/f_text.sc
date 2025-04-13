#ifdef GL_ES
precision mediump float;
varying vec2 v_texcoord0;
#else
in vec2 v_texcoord0;
#endif

#include <bgfx_shader.sh>

// Diffuse texture passed in from CPU — this will contain the rendered text
uniform sampler2D u_diffuseTex;
uniform vec4 u_comicColor;      // User-controlled text color.

void main()
{
    // Sample from the diffuse texture using UVs
    vec4 texColor = texture2D(u_diffuseTex, v_texcoord0);

    // Multiply sampled texture with custom color
    gl_FragColor = texColor * u_comicColor;
}
