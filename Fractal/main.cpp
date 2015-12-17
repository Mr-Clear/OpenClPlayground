#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER) ||  defined(_WINDOWS_) || defined(__WIN32__) || defined(__WINDOWS__)
#define WINDOWS_OS
#elif defined(__APPLE__) || defined(__MACH__)
#define APPLE_OS
#else
#define LINUX_OS
#endif

#include <GL/glew.h>

#include "OpenCLUtil.h"
#define __CL_ENABLE_EXCEPTIONS
#include "cl.hpp"

#ifdef WINDOWS_OS
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#endif

#ifdef LINUX_OS
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "OpenGLUtil.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;
using namespace cl;

typedef unsigned int uint;

static int wind_width = 640;
static int wind_height= 480;
static int channels   = 4;

static const float matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f};


static const float vertices[12] = {
    -1.0f,-1.0f,0.0,
    1.0f,-1.0f,0.0,
    1.0f, 1.0f,0.0,
    -1.0f, 1.0f,0.0};
static const float texcords[8] = {0.0,1.0,1.0,1.0,1.0,0.0,0.0,0.0};
static const uint indices[6] = {0,1,2,0,2,3};

typedef struct {
    Device d;
    CommandQueue q;
    Program p;
    Kernel k;
    Buffer i;
    ImageGL tex;
    cl::size_t<3> dims;
} process_params;

typedef struct {
    GLuint prg;
    GLuint vao;
    GLuint tex;
} render_params;

process_params params;
render_params rparams;

static void glfw_error_callback(int error, const char* desc)
{
    fputs(desc,stderr);
}

static void glfw_key_callback(GLFWwindow* wind, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(wind, GL_TRUE);
    }
}

static void glfw_framebuffer_size_callback(GLFWwindow* wind, int width, int height)
{
    glViewport(0,0,width,height);
}

void processTimeStep(void);
void renderFrame(void);

int main(void)
{
    GLFWwindow* window;

    if (!glfwInit())
        return 255;

    glfwSetErrorCallback(glfw_error_callback);

    window = glfwCreateWindow(wind_width,wind_height,"CLGL",NULL,NULL);
    if (!window) {
        glfwTerminate();
        return 254;
    }

    glfwMakeContextCurrent(window);
    GLenum res = glewInit();
    if (res!=GLEW_OK) {
        std::cout<<"Error Initializing GLEW | Exiting"<<std::endl;
        return 253;
    }

    cl_int errCode;
    try {
        Platform lPlatform = getPlatform();
        // Select the default platform and create a context using this platform and the GPU
#ifdef LINUX_OS
        cl_context_properties cps[] = {
            CL_GL_CONTEXT_KHR, (cl_context_properties)glfwGetGLXContext(window),
            CL_GLX_DISPLAY_KHR, (cl_context_properties)glfwGetX11Display(),
            CL_CONTEXT_PLATFORM, (cl_context_properties)lPlatform(),
            0
        };
#endif
#ifdef WINDOWS_OS
        cl_context_properties cps[] = {
            CL_GL_CONTEXT_KHR, (cl_context_properties)glfwGetWGLContext(window),
            CL_WGL_HDC_KHR, (cl_context_properties)GetDC(glfwGetWin32Window(window)),
            CL_CONTEXT_PLATFORM, (cl_context_properties)lPlatform(),
            0
        };
#endif
        Context context(CL_DEVICE_TYPE_GPU, cps);
        // Get a list of devices on this platform
        std::vector<Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
        params.d = devices[0];
        if (!checkExtnAvailability(params.d,CL_GL_SHARING_EXT)) {
            return 251;
        }
        // Create a command queue and use the first device
        params.q = CommandQueue(context, params.d);
#ifdef WINDOWS_OS
        params.p = getProgram(context,"fractal.cl",errCode);
#else
        params.p = getProgram(context,"./Fractal/fractal.cl",errCode);
#endif
        params.p.build(devices);
        params.k = Kernel(params.p, "fractal");
        // create opengl stuff
#ifdef WINDOWS_OS
        rparams.prg = initShaders("vertex.glsl","fragment.glsl");
#else
        rparams.prg = initShaders("./Fractal/vertex.glsl","./Fractal/fragment.glsl");
#endif
        rparams.tex = createTexture2D(wind_width,wind_height);
        GLuint vbo  = createBuffer(12,vertices,GL_STATIC_DRAW);
        GLuint tbo  = createBuffer(8,texcords,GL_STATIC_DRAW);
        GLuint ibo;
        glGenBuffers(1,&ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(uint)*6,indices,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        // bind vao
        glGenVertexArrays(1,&rparams.vao);
        glBindVertexArray(rparams.vao);
        // attach vbo
        glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,NULL);
        glEnableVertexAttribArray(0);
        // attach tbo
        glBindBuffer(GL_ARRAY_BUFFER,tbo);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,0,NULL);
        glEnableVertexAttribArray(1);
        // attach ibo
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);
        glBindVertexArray(0);
        // create opengl texture reference using opengl texture
        params.tex = ImageGL(context,CL_MEM_READ_WRITE,GL_TEXTURE_2D,0,rparams.tex,&errCode);
        if (errCode!=CL_SUCCESS) {
            std::cout<<"Failed to create OpenGL texture refrence: "<<errCode<<std::endl;
            return 250;
        }
        // create opencl input and output buffers
        params.i = Buffer(context,CL_MEM_READ_ONLY,sizeof(cl_uchar4)*wind_width*wind_height);
        params.dims[0] = wind_width;
        params.dims[1] = wind_height;
        params.dims[2] = 1;
    } catch(Error error) {
        std::cout << error.what() << "(" << error.err() << ")" << std::endl;
        std::string val = params.p.getBuildInfo<CL_PROGRAM_BUILD_LOG>(params.d);
        std::cout<<"Log:\n"<<val<<std::endl;
        return 249;
    }

    glfwSetKeyCallback(window,glfw_key_callback);
    glfwSetFramebufferSizeCallback(window,glfw_framebuffer_size_callback);

    while (!glfwWindowShouldClose(window)) {
        // process call
        processTimeStep();
        // render call
        renderFrame();
        // swap front and back buffers
        glfwSwapBuffers(window);
        // poll for events
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    return 0;
}

inline unsigned divup(unsigned a, unsigned b)
{
    return (a+b-1)/b;
}

void processTimeStep()
{
    cl::Event ev;
    try {
        glFinish();

        std::vector<Memory> objs;
        objs.clear();
        objs.push_back(params.tex);
        // flush opengl commands and wait for object acquisition
        cl_int res = params.q.enqueueAcquireGLObjects(&objs,NULL,&ev);
        ev.wait();
        if (res!=CL_SUCCESS) {
            std::cout<<"Failed acquiring GL object: "<<res<<std::endl;
            exit(248);
        }
        NDRange local(16, 16);
        NDRange global( 16 * divup(params.dims[0], 16),
                        16 * divup(params.dims[1], 16));
        // set kernel arguments
        params.k.setArg(0,params.tex);
        params.k.setArg(1,params.i);
        params.k.setArg(2,(int)params.dims[0]);
        params.k.setArg(3,(int)params.dims[1]);
        params.k.setArg(4,(int)params.dims[2]);
        params.q.enqueueNDRangeKernel(params.k,cl::NullRange, global, local);
        // release opengl object
        res = params.q.enqueueReleaseGLObjects(&objs);
        ev.wait();
        if (res!=CL_SUCCESS) {
            std::cout<<"Failed releasing GL object: "<<res<<std::endl;
            exit(247);
        }
        params.q.finish();
    } catch(Error err) {
        std::cout << err.what() << "(" << err.err() << ")" << std::endl;
    }
}

void renderFrame()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.2,0.2,0.2,0.0);
    glEnable(GL_DEPTH_TEST);
    // bind shader
    glUseProgram(rparams.prg);
    // get uniform locations
    int mat_loc = glGetUniformLocation(rparams.prg,"matrix");
    int tex_loc = glGetUniformLocation(rparams.prg,"tex");
    // bind texture
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(tex_loc,0);
    glBindTexture(GL_TEXTURE_2D,rparams.tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    // set project matrix
    glUniformMatrix4fv(mat_loc,1,GL_FALSE,matrix);
    // now render stuff
    glBindVertexArray(rparams.vao);
    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
}
