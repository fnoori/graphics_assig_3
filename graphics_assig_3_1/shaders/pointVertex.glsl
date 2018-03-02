// ==========================================================================
// Vertex program for barebones GLFW boilerplate
//
// Author:  Sonny Chan, University of Calgary
// Date:    December 2015
// ==========================================================================
#version 410

// location indices for these attributes correspond to those specified in the
// InitializeGeometry() function of the main program
layout(location = 0) in vec2 VertexPosition;
layout(location = 1) in vec3 VertexColour;

uniform float scaleBy;
uniform float shiftBy;

out vec3 Colour;

void main()
{
    // assign vertex position without modification
    gl_Position = vec4((VertexPosition + shiftBy) * scaleBy, 0, 1);
    gl_PointSize = 8.f;
    
    // assign output colour to be interpolated
    Colour = VertexColour;
}
