#ifdef GL_ES
precision mediump float;
varying vec4 v_color; // Not used for picking, but declared for consistency.
#else
in vec4 v_color;
#endif

#include <bgfx_shader.sh>

// Output color (BGFX requires OUTPUT0)
OUTPUT0(vec4 colorOut);

// This uniform holds the flat color used to encode the object ID.
uniform vec4 u_id;

void main()
{
    // Output the uniform u_id so that the rendered fragment color encodes the object’s ID.
    colorOut = u_id;
}