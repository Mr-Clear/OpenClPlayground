#include <glad/glad.h>

#include <common/OpenCLUtil.h>

#ifdef OS_WIN
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#endif

#ifdef OS_LNX
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif

#include "common/Exception.h"

#include <fmt/core.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <common/OpenGLUtil.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>

using namespace std;
using namespace cl;

using uint = unsigned int;

static int constexpr wind_width = 640;
static int constexpr wind_height= 480;
static float constexpr percent  = 0.02;
static int constexpr nparticles = percent * wind_width * wind_height;

static const std::array<float, 16> matrix =
{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

struct process_params
{
    Device device;
    CommandQueue commandQueue;
    Program program;
    Kernel kernel;
    Buffer buffer;
    std::array<size_t, 3> dims = {};
};

struct render_params
{
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    BufferGL tmp;
};

process_params params;
render_params rparams;

static void glfw_error_callback(int error, const char* desc)
{
    fputs(desc, stderr);
}

static void glfw_key_callback(GLFWwindow* wind, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(wind, GL_TRUE);
    }
}

static void glfw_framebuffer_size_callback(GLFWwindow* wind, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processTimeStep();
void renderFrame();

int main(int, char*[])
{
    GLFWwindow* window;

    if (!glfwInit())
        return 255;

    glfwSetErrorCallback(glfw_error_callback);

    window = glfwCreateWindow(wind_width, wind_height, "Random particles", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 254;
    }

    glfwMakeContextCurrent(window);
    if(!gladLoadGL())
        throw Exception("gladLoadGL failed!\n");
    cout << fmt::format("OpenGL {}.{}", GLVersion.major, GLVersion.minor) << endl;

    cl_int errCode;
    try
    {
        Platform lPlatform = getPlatform();
        // Select the default platform and create a context using this platform and the GPU
#ifdef OS_LNX
        std::array<cl_context_properties, 7> cps =
        {
            CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(glfwGetGLXContext(window)),
            CL_GLX_DISPLAY_KHR, reinterpret_cast<cl_context_properties>(glfwGetX11Display()),
            CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(lPlatform()),
            0
        };
#endif
#ifdef OS_WIN
        std::array<cl_context_properties, 7> cps =
        {
            CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(glfwGetWGLContext(window)),
            CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(GetDC(glfwGetWin32Window(window))),
            CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(lPlatform()),
            0
        };
#endif
        std::vector<Device> devices;
        lPlatform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
        // Get a list of devices on this platform
        for (auto & device : devices)
        {
            if (checkExtnAvailability(device, CL_GL_SHARING_EXT))
            {
                params.device = device;
                break;
            }
        }
        Context context(params.device, cps.data());
        // Create a command queue and use the first device
        params.commandQueue = CommandQueue(context, params.device);
        params.program = getProgram(context, ASSETS_DIR"/random.cl", errCode);
        params.program.build(std::vector<Device>(1, params.device));
        params.kernel = Kernel(params.program, "random");
        // create opengl stuff
        rparams.program = initShaders(ASSETS_DIR"/partsim.vert", ASSETS_DIR"/partsim.frag");

        std::random_device rd;
        std::mt19937 eng(rd());
        std::uniform_real_distribution<> distWidth(10, 100);
        std::uniform_real_distribution<> distHeight(10, 100);
        std::array<float, 2 * nparticles> data{};
        for(int n = 0; n < nparticles; ++n)
        {
            data[2 * n + 0] = distWidth(eng);
            data[2 * n + 1] = distHeight(eng);
        }

        rparams.vbo = createBuffer(data.size(), data.data(), GL_DYNAMIC_DRAW);
        rparams.tmp = BufferGL(context, CL_MEM_READ_WRITE, rparams.vbo, nullptr);
        // bind vao
        glGenVertexArrays(1, &rparams.vao);
        glBindVertexArray(rparams.vao);
        // attach vbo
        glBindBuffer(GL_ARRAY_BUFFER, rparams.vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        // create opencl input and output buffer
        params.buffer = Buffer(context, CL_MEM_READ_WRITE, sizeof(data));
        params.commandQueue.enqueueWriteBuffer(params.buffer, CL_TRUE, 0, sizeof(data), data.data());
        params.dims[0] = nparticles;
        params.dims[1] = 1;
        params.dims[2] = 1;
    }
    catch(Error error)
    {
        std::cout << error.what() << "(" << error.err() << ")" << std::endl;
        std::string val = params.program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(params.device);
        std::cout << "Log:\n" << val << std::endl;
        return 249;
    }

    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);

    while (!glfwWindowShouldClose(window))
    {
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
    return (a + b - 1) / b;
}

void processTimeStep()
{
    static int seed = std::rand();
    cl::Event ev;
    try
    {
        NDRange local(16);
        NDRange global(16 * divup(params.dims[0], 16));
        // set kernel arguments
        params.kernel.setArg(0, params.buffer);
        params.kernel.setArg(1, wind_width);
        params.kernel.setArg(2, wind_height);
        params.kernel.setArg(3, ++seed);
        params.commandQueue.enqueueNDRangeKernel(params.kernel, cl::NullRange, global, local);

        glFinish();
        std::vector<Memory> objs;
        objs.clear();
        objs.push_back(rparams.tmp);
        // flush opengl commands and wait for object acquisition
        cl_int res = params.commandQueue.enqueueAcquireGLObjects(&objs, nullptr, &ev);
        ev.wait();
        if (res != CL_SUCCESS)
        {
            std::cout << "Failed acquiring GL object: " << res << std::endl;
            exit(248);
        }
        params.commandQueue.enqueueCopyBuffer(params.buffer, rparams.tmp, 0, 0, 2 * nparticles * sizeof(float), nullptr, nullptr);
        // release opengl object
        res = params.commandQueue.enqueueReleaseGLObjects(&objs);
        ev.wait();
        if (res != CL_SUCCESS)
        {
            std::cout << "Failed releasing GL object: " << res << std::endl;
            exit(247);
        }
        params.commandQueue.finish();
    }
    catch(Error err)
    {
        std::cout << err.what() << "(" << err.err() << ")" << std::endl;
    }
}

void renderFrame()
{
    static const std::array<float, 4> pcolor = {1.0, 0.0, 0.0, 1.0};
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.2, 0.2, 0.2, 1.0);
    glEnable(GL_DEPTH_TEST);
    // bind shader
    glUseProgram(rparams.program);
    // get uniform locations
    int mat_loc = glGetUniformLocation(rparams.program, "matrix");
    int col_loc = glGetUniformLocation(rparams.program, "color");
    // set project matrix
    glUniformMatrix4fv(mat_loc, 1, GL_FALSE, matrix.data());
    glUniform4fv(col_loc, 1, pcolor.data());
    // now render stuff
    glPointSize(4);
    glBindVertexArray(rparams.vao);
    glDrawArrays(GL_POINTS, 0, nparticles);
    glBindVertexArray(0);
}
