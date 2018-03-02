// ==========================================================================
// Barebones OpenGL Core Profile Boilerplate
//    using the GLFW windowing system (http://www.glfw.org)
//
// Loosely based on
//  - Chris Wellons' example (https://github.com/skeeto/opengl-demo) and
//  - Camilla Berglund's example (http://www.glfw.org/docs/latest/quick.html)
//
// Author:  Sonny Chan, University of Calgary
// Co-Authors:
//            Jeremy Hart, University of Calgary
//            John Hall, University of Calgary
// Date:    December 2015
// ==========================================================================

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <iterator>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "texture.h"
#include "fonts/GlyphExtractor.h"

using namespace std;
using namespace glm;
// --------------------------------------------------------------------------
// OpenGL utility and support function prototypes

void QueryGLVersion();
bool CheckGLErrors();

enum BezierCurve
{
    CUBIC,
    QUADRATIC
};

enum FontLoaded
{
    LORA_BOLD_ITALIC,
    INCONSOLATA,
    QARMIC_SANS,
    ALEX_BRUSH,
    NO_FONT
};

string LoadSource(const string &filename);
GLuint CompileShader(GLenum shaderType, const string &source);
GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader, GLuint tcsShader, GLuint tesShader);
void addVertices(BezierCurve type);
void addColours();
float insertGlyphCharacter(char charToInsert, float advance, float shiftToCentre, float scaleToFit);
void insertString(string text);
void loadLoraBoldItalic();
void loadInconsolata();
void loadQarmicSans();
void loadAlexBrush();
mat4 translateText(mat4 transform);

vector<vec2> vertices;
vector<vec3> colours;

float cubicBezier;
float quadraticBezier;
float drawPoints;

FontLoaded fontLoaded;
float scaleBy;
float shiftBy;
float fontShiftBy;
float fontYShiftBy;
float fontScaleBy;
float resetScroll;
BezierCurve bezierType;
mat4 transformVertices = mat4(1.0f);

float origLocation = 0.f;
bool textIsScrolling = false;
float textScrollSpeed = 0.05;

GlyphExtractor glyphExtractor;
MyGlyph myGlyph;

// --------------------------------------------------------------------------
// Functions to set up OpenGL shader programs for rendering

// load, compile, and link shaders, returning true if successful
GLuint InitializeShaders()
{
    // load shader source from files
    string vertexSource = LoadSource("shaders/vertex.glsl");
    string fragmentSource = LoadSource("shaders/fragment.glsl");
    string tcsSource = LoadSource("shaders/tessControl.glsl");
    string tesSource = LoadSource("shaders/tessEval.glsl");
    if (vertexSource.empty() || fragmentSource.empty()) return 0;
    
    // compile shader source into shader objects
    GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint tcs = CompileShader(GL_TESS_CONTROL_SHADER, tcsSource);
    GLuint tes = CompileShader(GL_TESS_EVALUATION_SHADER, tesSource);
    
    // link shader program
    GLuint program = LinkProgram(vertex, fragment, tcs, tes);
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    glDeleteShader(tcs);
    glDeleteShader(tes);
    
    if (CheckGLErrors())
        return 0;
    
    // check for OpenGL errors and return false if error occurred
    return program;
}

GLuint initializePointShaders()
{
    string pointVertexSource = LoadSource("shaders/pointVertex.glsl");
    string pointFragmentSource = LoadSource("shaders/pointFragment.glsl");
    
    if (pointVertexSource.empty() || pointFragmentSource.empty()) {
        return 0;
    }
    
    GLuint vertex = CompileShader(GL_VERTEX_SHADER, pointVertexSource);
    GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, pointFragmentSource);
    
    GLuint program = LinkProgram(vertex, fragment, GL_FALSE, GL_FALSE);
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    if (CheckGLErrors()) {
        return 0;
    }
    
    return program;
}

// --------------------------------------------------------------------------
// Functions to set up OpenGL buffers for storing geometry data

struct Geometry
{
    // OpenGL names for array buffer objects, vertex array object
    GLuint  vertexBuffer;
    GLuint  textureBuffer;
    GLuint  colourBuffer;
    GLuint  vertexArray;
    GLsizei elementCount;
    
    // initialize object names to zero (OpenGL reserved value)
    Geometry() : vertexBuffer(0), colourBuffer(0), vertexArray(0), elementCount(0)
    {}
};

bool InitializeVAO(Geometry *geometry){
    
    const GLuint VERTEX_INDEX = 0;
    const GLuint COLOUR_INDEX = 1;
    
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    //Generate Vertex Buffer Objects
    // create an array buffer object for storing our vertices
    glGenBuffers(1, &geometry->vertexBuffer);
    
    // create another one for storing our colours
    glGenBuffers(1, &geometry->colourBuffer);
    
    //Set up Vertex Array Object
    // create a vertex array object encapsulating all our vertex attributes
    glGenVertexArrays(1, &geometry->vertexArray);
    glBindVertexArray(geometry->vertexArray);
    
    // associate the position array with the vertex array object
    glBindBuffer(GL_ARRAY_BUFFER, geometry->vertexBuffer);
    glVertexAttribPointer(
                          VERTEX_INDEX,         //Attribute index
                          2,                    //# of components
                          GL_FLOAT,             //Type of component
                          GL_FALSE,             //Should be normalized?
                          0,                    //Stride - can use 0 if tightly packed
                          0);                   //Offset to first element
    glEnableVertexAttribArray(VERTEX_INDEX);
    
    // associate the colour array with the vertex array object
    glBindBuffer(GL_ARRAY_BUFFER, geometry->colourBuffer);
    glVertexAttribPointer(
                          COLOUR_INDEX,         //Attribute index
                          3,                    //# of components
                          GL_FLOAT,             //Type of component
                          GL_FALSE,             //Should be normalized?
                          0,                    //Stride - can use 0 if tightly packed
                          0);                   //Offset to first element
    glEnableVertexAttribArray(COLOUR_INDEX);
    
    // unbind our buffers, resetting to default state
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    return !CheckGLErrors();
}

// create buffers and fill with geometry data, returning true if successful
bool LoadGeometry(Geometry *geometry, int elementCount)
{
    geometry->elementCount = elementCount;
    
    // create an array buffer object for storing our vertices
    glBindBuffer(GL_ARRAY_BUFFER, geometry->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * geometry->elementCount, &vertices[0], GL_STATIC_DRAW);
    
    // create another one for storing our colours
    glBindBuffer(GL_ARRAY_BUFFER, geometry->colourBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * geometry->elementCount, &colours[0], GL_STATIC_DRAW);
    
    //Unbind buffer to reset to default state
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // check for OpenGL errors and return false if error occurred
    return !CheckGLErrors();
}

// deallocate geometry-related objects
void DestroyGeometry(Geometry *geometry)
{
    // unbind and destroy our vertex array object and associated buffers
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &geometry->vertexArray);
    glDeleteBuffers(1, &geometry->vertexBuffer);
    glDeleteBuffers(1, &geometry->colourBuffer);
}

// --------------------------------------------------------------------------
// Rendering function that draws our scene to the frame buffer

void RenderScene(Geometry *geometry, GLuint program, GLuint pointProgram)
{
    // clear screen to a dark grey colour
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // bind our shader program and the vertex array object containing our
    // scene geometry, then tell OpenGL to draw our geometry

    glUseProgram(program);
    glBindVertexArray(geometry->vertexArray);
    
    unsigned int isQuadratic = glGetUniformLocation(program, "quadratic");
    glUniform1f(isQuadratic, quadraticBezier);
    
    unsigned int isCubic = glGetUniformLocation(program, "cubic");
    glUniform1f(isCubic, cubicBezier);
    
    unsigned int isPoints = glGetUniformLocation(program, "drawControlPoints");
    glUniform1f(isPoints, drawPoints);
    
    unsigned int scaleIs = glGetUniformLocation(program, "scaleBy");
    glUniform1f(scaleIs, scaleBy);
    
    unsigned int shiftIs = glGetUniformLocation(program, "shiftBy");
    glUniform1f(shiftIs, shiftBy);
    
    glDrawArrays(GL_PATCHES, 0, geometry->elementCount);
    
    // reset state to default (no shader or geometry bound)
    glBindVertexArray(0);
    glUseProgram(0);
    
    // draw points and polygon lines only if drawing the figures
    if (fontLoaded == NO_FONT) {
        glUseProgram(pointProgram);
        glBindVertexArray(geometry->vertexArray);
        unsigned int scaleIsPoint = glGetUniformLocation(pointProgram, "scaleBy");
        glUniform1f(scaleIsPoint, scaleBy);
        
        unsigned int shiftIsPoint = glGetUniformLocation(pointProgram, "shiftBy");
        glUniform1f(shiftIsPoint, shiftBy);
        
        glDrawArrays(GL_POINTS, 0, geometry->elementCount);
        
        glBindVertexArray(0);
        glUseProgram(0);
        
        glUseProgram(pointProgram);
        glBindVertexArray(geometry->vertexArray);
        unsigned int scaleIsPointLines = glGetUniformLocation(pointProgram, "scaleBy");
        glUniform1f(scaleIsPointLines, scaleBy);
        
        unsigned int shiftIsPointLines = glGetUniformLocation(pointProgram, "shiftBy");
        glUniform1f(shiftIsPointLines, shiftBy);
        
        glDrawArrays(GL_LINES, 0, geometry->elementCount);
        
        glBindVertexArray(0);
        glUseProgram(0);
    }
    
    // check for an report any OpenGL errors
    CheckGLErrors();
}

mat4 translateText(mat4 transform)
{
    return translate(transform, vec3(0.1, 0.0f, 0.0f));
}

// --------------------------------------------------------------------------
// GLFW callback functions

// reports GLFW errors
void ErrorCallback(int error, const char* description)
{
    cout << "GLFW ERROR " << error << ":" << endl;
    cout << description << endl;
}

// handles keyboard input events
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        
        glfwSetWindowShouldClose(window, GL_TRUE);
        
    } else if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
        // For cup
        
        fontLoaded = NO_FONT;
        colours.clear();
        bezierType = QUADRATIC;
        addVertices(bezierType);
        addColours();
        cubicBezier = 0.f;
        quadraticBezier = 1.f;
        scaleBy = 0.35f;
        shiftBy = 0.f;
        textIsScrolling = false;
        glPatchParameteri(GL_PATCH_VERTICES, 3);
        
    } else if (key == GLFW_KEY_W && action == GLFW_PRESS) {
        // For fish
        
        fontLoaded = NO_FONT;
        colours.clear();
        bezierType = CUBIC;
        addVertices(bezierType);
        addColours();
        cubicBezier = 1.f;
        quadraticBezier = 0.f;
        scaleBy = 0.125f;
        shiftBy = -4.5f;
        textIsScrolling = false;
        glPatchParameteri(GL_PATCH_VERTICES, 4);
        
    } else if (key == GLFW_KEY_A && action == GLFW_PRESS) {
        // For inconsolata
        fontLoaded = INCONSOLATA;
        fontYShiftBy = -0.4;
        textIsScrolling = false;
        loadInconsolata();
        
    } else if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        // For lora bold italic
        fontLoaded = LORA_BOLD_ITALIC;
        fontYShiftBy = -0.4;
        textIsScrolling = false;
        loadLoraBoldItalic();
        
    } else if (key == GLFW_KEY_D && action == GLFW_PRESS) {
        // For my font (variatino of comic sans), qarmic_sans_abridged
        fontLoaded = QARMIC_SANS;
        fontYShiftBy = -0.4;
        textIsScrolling = false;
        loadQarmicSans();
        
    } else if (key == GLFW_KEY_Z && action == GLFW_PRESS) {
        // Srolling text with 'Alex-brush'
        fontLoaded = ALEX_BRUSH;
        textIsScrolling = true;
        fontShiftBy = -0.4;
        fontYShiftBy = -0.3;
        fontScaleBy = 1.2f;
        resetScroll = -14.9499;
        loadAlexBrush();
        
    } else if (key == GLFW_KEY_X && action == GLFW_PRESS) {
        // Scrolling text with 'Inconsolata'
        fontLoaded = INCONSOLATA;
        textIsScrolling = true;
        fontShiftBy = -0.4;
        fontYShiftBy = -0.3;
        fontScaleBy = 1.1f;
        resetScroll = -19.965;
        loadInconsolata();
        
    } else if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        // Scrolling text with 'Qarmic Sans abridged'
        fontLoaded = QARMIC_SANS;
        textIsScrolling = true;
        fontShiftBy = -0.4;
        fontYShiftBy = -0.4;
        fontScaleBy = 0.9f;
        resetScroll = -25.0238;
        loadQarmicSans();
        
    } else if (key == GLFW_KEY_O && action == GLFW_PRESS) {
        
        if (textScrollSpeed <= 0.01) {
            textScrollSpeed = 0.01;
        } else {
            textScrollSpeed -= 0.01;
        }
        
    } else if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        
        textScrollSpeed += 0.01;
        
    }
}

void loadLoraBoldItalic()
{
    if (!glyphExtractor.LoadFontFile("fonts/lora/Lora-BoldItalic.ttf")) {
        cout << "Failed to load 'alex-brush.ttf' file" << endl;
    }
    
    vertices.clear();
    colours.clear();
    bezierType = QUADRATIC;
    string toPass = "Farzam Noori";
    fontShiftBy = -3.f;
    fontScaleBy = 0.25;
    insertString(toPass);
    
    for (int i = 0; i < vertices.size(); i++) {
        colours.push_back(vec3(1.f, 1.f, 1.f));
    }
    
    cubicBezier = 0.f;
    quadraticBezier = 1.f;
    scaleBy = 1;
    shiftBy = 0;
    glPatchParameteri(GL_PATCH_VERTICES, 3);
}

void loadInconsolata()
{
    if (!glyphExtractor.LoadFontFile("fonts/source-sans-pro/SourceSansPro-SemiboldIt.otf")) {
        cout << "Failed to load 'Inconsolata.otf' file" << endl;
    }
    
    vertices.clear();
    colours.clear();
    bezierType = CUBIC;
    string toPass;
    
    if (textIsScrolling) {
        toPass = "The quick brown fox jumps over the lazy dog.";
        if (fontShiftBy > resetScroll) {
            fontShiftBy -= 0.1*textScrollSpeed;
        } else {
            fontShiftBy = -0.3;
        }
    } else {
        toPass = "Farzam Noori";
        fontShiftBy = -2.7f;
        fontScaleBy = 0.30;
    }
    
    insertString(toPass);
    
    for (int i = 0; i < vertices.size(); i++) {
        colours.push_back(vec3(1.f, 1.f, 1.f));
    }
    
    cubicBezier = 1.f;
    quadraticBezier = 0.f;
    scaleBy = 1;
    shiftBy = 0;
    glPatchParameteri(GL_PATCH_VERTICES, 4);
}

void loadQarmicSans()
{
    if (!glyphExtractor.LoadFontFile("fonts/Qarmic_sans_Abridged.ttf")) {
        cout << "Failed to load 'alex-brush.ttf' file" << endl;
    }
    
    vertices.clear();
    colours.clear();
    bezierType = QUADRATIC;
    string toPass;
    
    if (textIsScrolling) {
        toPass = "The quick brown fox jumps over the lazy dog.";
        if (fontShiftBy > resetScroll) {
            fontShiftBy -= 0.1*textScrollSpeed;
        } else {
            fontShiftBy = -0.3;
        }
    } else {
        toPass = "Farzam Noori";
        fontShiftBy = -3.3f;
        fontScaleBy = 0.25;
    }
    
    insertString(toPass);
    
    for (int i = 0; i < vertices.size(); i++) {
        colours.push_back(vec3(1.f, 1.f, 1.f));
    }
    
    cubicBezier = 0.f;
    quadraticBezier = 1.f;
    scaleBy = 1;
    shiftBy = 0;
    glPatchParameteri(GL_PATCH_VERTICES, 3);
}

void loadAlexBrush()
{
    if (!glyphExtractor.LoadFontFile("fonts/alex-brush/AlexBrush-Regular.ttf")) {
        cout << "Failed to load 'alex-brush.ttf' file" << endl;
    }
    
    vertices.clear();
    colours.clear();
    bezierType = QUADRATIC;
    string toPass;
    
    if (textIsScrolling) {
        toPass = "The quick brown fox jumps over the lazy dog.";
        if (fontShiftBy > resetScroll) {
            fontShiftBy -= 0.1*textScrollSpeed;
        } else {
            fontShiftBy = -0.3;
        }
    } else {
        toPass = "Farzam Noori";
        fontShiftBy = -2.5f;
        fontScaleBy = 0.30;
    }
    
    insertString(toPass);
    
    for (int i = 0; i < vertices.size(); i++) {
        colours.push_back(vec3(1.f, 1.f, 1.f));
    }
    
    cubicBezier = 0.f;
    quadraticBezier = 1.f;
    scaleBy = 1;
    shiftBy = 0;
    glPatchParameteri(GL_PATCH_VERTICES, 3);
}

// ==========================================================================
// PROGRAM ENTRY POINT

int main(int argc, char *argv[])
{
    // initialize the GLFW windowing system
    if (!glfwInit()) {
        cout << "ERROR: GLFW failed to initialize, TERMINATING" << endl;
        return -1;
    }
    glfwSetErrorCallback(ErrorCallback);
    
    // attempt to create a window with an OpenGL 4.1 core profile context
    GLFWwindow *window = 0;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    int width = 512, height = 512;
    window = glfwCreateWindow(width, height, "CPSC 453 OpenGL Boilerplate", 0, 0);
    if (!window) {
        cout << "Program failed to create GLFW window, TERMINATING" << endl;
        glfwTerminate();
        return -1;
    }
    
    // set keyboard callback function and make our context current (active)
    glfwSetKeyCallback(window, KeyCallback);
    glfwMakeContextCurrent(window);
    
    //Intialize GLAD
    if (!gladLoadGL())
    {
        cout << "GLAD init failed" << endl;
        return -1;
    }
    
    // query and print out information about our OpenGL environment
    QueryGLVersion();
    
    // call function to load and compile shader programs
    GLuint program = InitializeShaders();
    if (program == 0) {
        cout << "Program could not initialize shaders, TERMINATING" << endl;
        return -1;
    }
    
    GLuint pointProgram = initializePointShaders();
    if (pointProgram == 0) {
        cout << "Point shaders failed to initialize, TERMINATING" << endl;
        return -1;
    }
    
    // call function to create and fill buffers with geometry data
    Geometry geometry;
    if (!InitializeVAO(&geometry)) {
        cout << "Program failed to intialize geometry!" << endl;
    }
    
    drawPoints = 1.f;
    
    // run an event-triggered main loop
    while (!glfwWindowShouldClose(window))
    {
        if(!LoadGeometry(&geometry, vertices.size())) {
            cout << "Failed to load geometry" << endl;
        }
        
        if (textIsScrolling && fontLoaded == ALEX_BRUSH) {
            loadAlexBrush();
        } else if (textScrollSpeed && fontLoaded == INCONSOLATA) {
            loadInconsolata();
        } else if (textScrollSpeed && fontLoaded == QARMIC_SANS) {
            loadQarmicSans();
        }
        
        // call function to draw our scene
        RenderScene(&geometry, program, pointProgram);
        
        glfwSwapBuffers(window);
        
        glfwPollEvents();
    }
    
    // clean up allocated resources before exit
    DestroyGeometry(&geometry);
    glUseProgram(0);
    glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    cout << "Goodbye!" << endl;
    return 0;
}

// ==========================================================================
// SUPPORT FUNCTION DEFINITIONS

// --------------------------------------------------------------------------
// OpenGL utility functions

void QueryGLVersion()
{
    // query opengl version and renderer information
    string version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    string glslver = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    string renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    
    cout << "OpenGL [ " << version << " ] "
    << "with GLSL [ " << glslver << " ] "
    << "on renderer [ " << renderer << " ]" << endl;
}

bool CheckGLErrors()
{
    bool error = false;
    for (GLenum flag = glGetError(); flag != GL_NO_ERROR; flag = glGetError())
    {
        cout << "OpenGL ERROR:  ";
        switch (flag) {
            case GL_INVALID_ENUM:
                cout << "GL_INVALID_ENUM" << endl; break;
            case GL_INVALID_VALUE:
                cout << "GL_INVALID_VALUE" << endl; break;
            case GL_INVALID_OPERATION:
                cout << "GL_INVALID_OPERATION" << endl; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                cout << "GL_INVALID_FRAMEBUFFER_OPERATION" << endl; break;
            case GL_OUT_OF_MEMORY:
                cout << "GL_OUT_OF_MEMORY" << endl; break;
            default:
                cout << "[unknown error code]" << endl;
        }
        error = true;
    }
    return error;
}

// --------------------------------------------------------------------------
// OpenGL shader support functions

// reads a text file with the given name into a string
string LoadSource(const string &filename)
{
    string source;
    
    ifstream input(filename.c_str());
    if (input) {
        copy(istreambuf_iterator<char>(input),
             istreambuf_iterator<char>(),
             back_inserter(source));
        input.close();
    }
    else {
        cout << "ERROR: Could not load shader source from file "
        << filename << endl;
    }
    
    return source;
}

// creates and returns a shader object compiled from the given source
GLuint CompileShader(GLenum shaderType, const string &source)
{
    // allocate shader object name
    GLuint shaderObject = glCreateShader(shaderType);
    
    // try compiling the source as a shader of the given type
    const GLchar *source_ptr = source.c_str();
    glShaderSource(shaderObject, 1, &source_ptr, 0);
    glCompileShader(shaderObject);
    
    // retrieve compile status
    GLint status;
    glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &length);
        string info(length, ' ');
        glGetShaderInfoLog(shaderObject, info.length(), &length, &info[0]);
        cout << "ERROR compiling shader:" << endl << endl;
        cout << source << endl;
        cout << info << endl;
    }
    
    return shaderObject;
}

// creates and returns a program object linked from vertex and fragment shaders
GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader, GLuint tcsShader, GLuint tesShader)
{
    // allocate program object name
    GLuint programObject = glCreateProgram();
    
    // attach provided shader objects to this program
    if (vertexShader)   glAttachShader(programObject, vertexShader);
    if (fragmentShader) glAttachShader(programObject, fragmentShader);
    if (tcsShader) glAttachShader(programObject, tcsShader);
    if (tesShader) glAttachShader(programObject, tesShader);
    
    // try linking the program with given attachments
    glLinkProgram(programObject);
    
    // retrieve link status
    GLint status;
    glGetProgramiv(programObject, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &length);
        string info(length, ' ');
        glGetProgramInfoLog(programObject, info.length(), &length, &info[0]);
        cout << "ERROR linking shader program:" << endl;
        cout << info << endl;
    }
    
    return programObject;
}

float insertGlyphCharacter(char charToInsert, float advance, float shiftToCentre, float scaleToFit)
{
    myGlyph = glyphExtractor.ExtractGlyph(charToInsert);
    
    for (int i = 0; i < myGlyph.contours.size(); i++) {
        
        MyContour myContour = myGlyph.contours[i];
        for (int j = 0; j < myContour.size(); j++) {

            MySegment mySegment = myContour[j];
            for (int k = 0; k <= mySegment.degree; k++) {
                
                if (mySegment.degree == 1 && bezierType == CUBIC) {
                    
                    vertices.push_back(vec2( (mySegment.x[k] + advance + shiftToCentre) * scaleToFit, (mySegment.y[k] + fontYShiftBy) * scaleToFit ));
                    vertices.push_back(vec2( (mySegment.x[k] + advance + shiftToCentre) * scaleToFit, (mySegment.y[k] + fontYShiftBy) * scaleToFit ));
                    
                } else if (mySegment.degree == 1 && bezierType == QUADRATIC) {
                    
                    if (k == 0) {
                        vertices.push_back(vec2( (mySegment.x[k] + advance + shiftToCentre) * scaleToFit, (mySegment.y[k] + fontYShiftBy) * scaleToFit  ));
                    }
                    vertices.push_back(vec2( (mySegment.x[k] + advance + shiftToCentre) * scaleToFit, (mySegment.y[k] + fontYShiftBy) * scaleToFit  ));
                    
                } else {
                    
                    vertices.push_back(vec2( (mySegment.x[k] + advance + shiftToCentre) * scaleToFit, (mySegment.y[k] + fontYShiftBy) * scaleToFit  ));
                    
                }
            }
        }
    }
    
    return myGlyph.advance;
}

void insertString(string text)
{
    int textLenght = text.length();
    char textCharArr[textLenght + 1];
    strcpy(textCharArr, text.c_str());
    
    float advanceBy = insertGlyphCharacter(textCharArr[0], 0, fontShiftBy, fontScaleBy);
    for (int i = 1; i < textLenght; i++) {
        advanceBy += insertGlyphCharacter(textCharArr[i], advanceBy, fontShiftBy, fontScaleBy);
    }
}

void addVertices(BezierCurve type)
{
    vertices.clear();
    
    if (type == CUBIC) {
        
        vertices.push_back(vec2(1, 1));
        vertices.push_back(vec2(4, 0));
        vertices.push_back(vec2(6, 2));
        vertices.push_back(vec2(9, 1));
        
        vertices.push_back(vec2(8, 2));
        vertices.push_back(vec2(0, 8));
        vertices.push_back(vec2(0, -2));
        vertices.push_back(vec2(8, 4));
        
        vertices.push_back(vec2(5, 3));
        vertices.push_back(vec2(3, 2));
        vertices.push_back(vec2(3, 3));
        vertices.push_back(vec2(5, 2));
        
        vertices.push_back(vec2(3, 2.2));
        vertices.push_back(vec2(3.5, 2.7));
        vertices.push_back(vec2(3.5, 3.3));
        vertices.push_back(vec2(3, 3.8));
        
        vertices.push_back(vec2(2.8, 3.5));
        vertices.push_back(vec2(2.4, 3.8));
        vertices.push_back(vec2(2.4, 3.2));
        vertices.push_back(vec2(2.8, 3.5));
        
    } else if (type == QUADRATIC) {
        
        vertices.push_back(vec2(1, 1));
        vertices.push_back(vec2(2, -1));
        vertices.push_back(vec2(0, -1));
        
        vertices.push_back(vec2(0, -1));
        vertices.push_back(vec2(-2, -1));
        vertices.push_back(vec2(-1, 1));
        
        vertices.push_back(vec2(-1, 1));
        vertices.push_back(vec2(0, 1));
        vertices.push_back(vec2(1, 1));
        
        vertices.push_back(vec2(1.2, 0.5));
        vertices.push_back(vec2(2.5, 1.0));
        vertices.push_back(vec2(1.3, -0.4));
    }
}

void addColours()
{
    colours.push_back(vec3( 1.0f, 0.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 1.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 0.0f, 1.0f ));
    colours.push_back(vec3( 1.0f, 0.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 1.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 0.0f, 1.0f ));
    colours.push_back(vec3( 1.0f, 0.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 1.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 0.0f, 1.0f ));
    colours.push_back(vec3( 1.0f, 0.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 1.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 0.0f, 1.0f ));
    colours.push_back(vec3( 0.0f, 0.0f, 1.0f ));
    colours.push_back(vec3( 1.0f, 0.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 1.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 0.0f, 1.0f ));
    colours.push_back(vec3( 1.0f, 0.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 1.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 0.0f, 1.0f ));
    colours.push_back(vec3( 1.0f, 0.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 1.0f, 0.0f ));
    colours.push_back(vec3( 0.0f, 0.0f, 1.0f ));
}
