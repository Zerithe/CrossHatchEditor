#ifdef GL_ES
precision mediump float;
varying vec4 v_position;
uniform vec4 u_depthScaleOffset; // (scale, offset, 0, 0)
#else
in vec4 v_position;
uniform vec4 u_depthScaleOffset;
#endif

#include <bgfx_shader.sh>

// Function to pack a float into RGBA.
vec4 packFloatToRgba(float depth) {
    const vec4 bitShifts = vec4(256.0*256.0*256.0, 256.0*256.0, 256.0, 1.0);
    const vec4 bitMask = vec4(0.0, 1.0/256.0, 1.0/256.0, 1.0/256.0);
    vec4 comp = fract(depth * bitShifts);
    comp -= comp.xxyz * bitMask;
    return comp;
}

void main()
{
    // Convert depth from clip-space to [0,1].
    float depth = (v_position.z / v_position.w) * u_depthScaleOffset.x + u_depthScaleOffset.y;
    gl_FragColor = packFloatToRgba(depth);
}
