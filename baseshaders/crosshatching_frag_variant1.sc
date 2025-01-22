$input v_normal, v_pos

#include <bgfx_shader.sh>

uniform vec4 u_lightDir;        // Light direction
uniform vec4 u_lightColor;      // Light color
uniform sampler2D u_hatchTex;   // Hatching texture

void main() {
    // Normalizing light direction and vertex normal
    vec3 lightDir = normalize(u_lightDir.xyz);
    vec3 normal = normalize(v_normal);
    
    // Calculate diffuse lighting
    float intensity = max(dot(normal, lightDir), 0.0);
	float crossHatchIntentsity = 0.0;
    
    // Quantize intensity to create cel shading effect
    if (intensity > 0.95) {
	//lightest
        intensity = 1.0;
		crossHatchIntentsity = 1.0;
    }
    else if (intensity > 0.5) {
        intensity = 0.6;
		crossHatchIntentsity = 0.6;
    }
    else if (intensity > 0.25) {
        intensity = 0.4;
		crossHatchIntentsity = 0.4;
    }
    else {
	//darkest
        intensity = 0.2;
		crossHatchIntentsity = 0.2;
    }

    // ** Crosshatch Effect **

    // UV scaling based on object size (can tweak scaling factor as needed)
    vec2 uv = v_pos.xy * 10.0;

    float line3 = abs(sin((uv.x + uv.y) * 3.1415 * 0.707)); // Diagonal lines
    float line4 = abs(sin((uv.x - uv.y) * 3.1415 * 0.707)); // Anti-diagonal lines
	
	
    // Combine the line patterns to create a crosshatch effect
    float combinedHatch = min(line3, line4);
    
    // Adjust the crosshatch visibility based on light intensity
    float hatchFactor = mix(1.0, combinedHatch, 1.0 - crossHatchIntentsity);
    
    // Final color combines cel-shading intensity with crosshatch effect
    vec3 finalColor = u_lightColor.rgb * intensity * hatchFactor;


    // Output final color
    gl_FragColor = vec4(finalColor, 1.0);
}