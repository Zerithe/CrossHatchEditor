#include <bgfx_shader.sh>

#ifdef GL_ES
precision mediump float;
#endif

// Vertex attributes
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texcoord0;

// Outputs to fragment shader
varying vec3 v_normal;
varying vec3 v_pos;
varying vec2 v_texcoord0;
varying vec3 v_viewDir;
varying vec3 v_lightDirs[16]; // Pre-calculate light directions for up to 16 lights
varying float v_lightDistances[16]; // Pre-calculate light distances

// Standard matrices needed for transformations
uniform mat4 u_modelViewProj;
uniform mat4 u_model;
uniform mat4 u_view;

// Light information
uniform vec4 u_lights[64];
uniform vec4 u_numLights; // x component holds the number of lights

// Camera position
uniform vec4 u_cameraPos;

void main() {
    // Transform the vertex position
    gl_Position = u_modelViewProj * vec4(a_position, 1.0);
    
    // Calculate world space position
    v_pos = (u_model * vec4(a_position, 1.0)).xyz;
    
    // Transform the normal to world space
    // Note: For non-uniformly scaled objects, you'd use the transpose of the inverse of the model matrix
    v_normal = normalize((u_model * vec4(a_normal, 0.0)).xyz);
    
    // Pass texture coordinates to fragment shader
    v_texcoord0 = a_texcoord0;
    
    // Calculate view direction (from vertex to camera)
    v_viewDir = normalize(u_cameraPos.xyz - v_pos);
    
    // Pre-calculate light directions and distances for each light
    int numLights = int(u_numLights.x);
    for (int i = 0; i < 16; i++) { // Use a constant here as loop variables in shaders need to be constant
        if (i >= numLights) break;
        
        int offset = i * 4;
        float lightType = u_lights[offset].x; // 0: directional, 1: point, 2: spot
        vec3 lightPos = u_lights[offset+1].xyz;
        vec3 lightDir = normalize(u_lights[offset+2].xyz);
        
        if (lightType == 0.0) {
            // Directional light
            v_lightDirs[i] = -lightDir;
            v_lightDistances[i] = 0.0; // No attenuation for directional lights
        } else {
            // Point or spot light
            v_lightDirs[i] = normalize(lightPos - v_pos);
            v_lightDistances[i] = length(lightPos - v_pos);
        }
    }
}