#version 410
/**
 * Tessellation Evaluation Shader
 *    Determines positions of points in tessellated patches
 *    Receives input from gl_in, tcs out variables and gl_TessCoord
 *    Run once for every vertex in the output patch
 */

//Type of patch being output
layout(isolines) in;

in vec3 teColour[];
//in gl_in[];

uniform float quadratic;
uniform float cubic;

uniform float scaleBy;
uniform float shiftBy;

//Information being sent out to fragment shader
//Will be interpolated as if sent from vertex shader
out vec3 Colour;

#define PI 3.14159265359

vec2 quadraticBezier(float u, vec2 p0, vec2 p1, vec2 p2)
{
    float b0 = (1.f - u) * (1.f - u);
    float b1 = 2.f * u * (1.f - u);
    float b2 = u * u;
    
    Colour = teColour[0] * b0 +
             teColour[1] * b1 +
             teColour[2] * b2;
    
    return (b0 * p0) + (b1 * p1) + (b2 * p2);
}

vec2 cubicBezier(float u, vec2 p0, vec2 p1, vec2 p2, vec2 p3)
{
    float b0 = (1. - u) * (1. - u) * (1. - u);
    float b1 = 3. * u * (1. - u) * (1. - u);
    float b2 = 3. * u * u * (1. - u);
    float b3 = u * u * u;
    
    Colour = teColour[0] * b0 +
             teColour[1] * b1 +
             teColour[2] * b2 +
             teColour[3] * b3;
    
    return (b0 * p0) + (b1 * p1) + (b2 * p2) + (b3 * p3);
}

void main()
{
    //gl_TessCoord.x will parameterize the segments of the line from 0 to 1
    //gl_TessCoord.y will parameterize the number of lines from 0 to 1
    float u = gl_TessCoord.x;
    
    vec3 startColour = teColour[0];
    vec3 endColour = teColour[1];
    
    vec2 p0 = gl_in[0].gl_Position.xy;
    vec2 p1 = gl_in[1].gl_Position.xy;
    vec2 p2 = gl_in[2].gl_Position.xy;
    vec2 p3 = gl_in[3].gl_Position.xy;
    
    vec2 position = vec2(0.f);
    if (cubic == 1) {
        position = cubicBezier(u, p0, p1, p2, p3);
    } else if (quadratic == 1) {
        position = quadraticBezier(u, p0, p1, p2);
    }
    
    gl_Position = vec4((position + shiftBy) * scaleBy, 0, 1);
}
