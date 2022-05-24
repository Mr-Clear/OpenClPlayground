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

using namespace std;
using namespace cl;

using uint = unsigned int;

static const uint NUM_JSETS = 9;

static const std::array<float, 16> matrix =
{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

static const std::array<float, 12> vertices =
{
    -1.0f, -1.0f, 0.0,
     1.0f, -1.0f, 0.0,
     1.0f, 1.0f, 0.0,
    -1.0f, 1.0f, 0.0
};

static const std::array<float, 8> texcords =
{
    0.0, 1.0,
    1.0, 1.0,
    1.0, 0.0,
    0.0, 0.0
};

static const std::array<uint, 6> indices = {0, 1, 2, 0, 2, 3};

static const std::array<float, 20> CJULIA =
{
    -0.700f, 0.270f,
    -0.618f, 0.000f,
    -0.400f, 0.600f,
     0.285f, 0.000f,
     0.285f, 0.010f,
     0.450f, 0.143f,
    -0.702f, -0.384f,
    -0.835f, -0.232f,
    -0.800f, 0.156f,
     0.279f, 0.000f
};

static int wind_width = 720;
static int wind_height= 720;
static int gJuliaSetIndex = 0;

struct process_params
{
    Device device;
    CommandQueue queue;
    Program pprogram;
    Kernel kernel;
    ImageGL tex;
    std::array<size_t, 3> dims = {};
};

struct render_params
{
    GLuint prg;
    GLuint vao;
    GLuint tex;
};

process_params params;
render_params rparams;

static void glfw_error_callback(int error, const char* desc)
{
    fputs(desc, stderr);
}

static void glfw_key_callback(GLFWwindow* wind, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(wind, GL_TRUE);
        else if (key == GLFW_KEY_1)
            gJuliaSetIndex = 0;
        else if (key == GLFW_KEY_2)
            gJuliaSetIndex = 1;
        else if (key == GLFW_KEY_3)
            gJuliaSetIndex = 2;
        else if (key == GLFW_KEY_4)
            gJuliaSetIndex = 3;
        else if (key == GLFW_KEY_5)
            gJuliaSetIndex = 4;
        else if (key == GLFW_KEY_6)
            gJuliaSetIndex = 5;
        else if (key == GLFW_KEY_7)
            gJuliaSetIndex = 6;
        else if (key == GLFW_KEY_8)
            gJuliaSetIndex = 7;
        else if (key == GLFW_KEY_9)
            gJuliaSetIndex = 8;
    }
}

static void glfw_framebuffer_size_callback(GLFWwindow* wind, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processTimeStep();
void renderFrame();

int main()
{
    if (!glfwInit())
        return 255;

          GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode    = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_RED_BITS    , mode->redBits    );
    glfwWindowHint(GLFW_GREEN_BITS  , mode->greenBits  );
    glfwWindowHint(GLFW_BLUE_BITS   , mode->blueBits   );
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    wind_width  = mode->width;
    wind_height = mode->height;

    GLFWwindow* window;

    glfwSetErrorCallback(glfw_error_callback);

    window = glfwCreateWindow(wind_width, wind_height, "Julia Sets", monitor, nullptr);
    if (!window) {
        glfwTerminate();
        return 254;
    }

    glfwMakeContextCurrent(window);

    if(!gladLoadGL())
        throw Exception("gladLoadGL failed!");
    cout << fmt::format("OpenGL {}.{}", GLVersion.major, GLVersion.minor) << endl;

    cl_int errCode;
    try {
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
        params.queue = CommandQueue(context, params.device);
        params.pprogram = getProgram(context, ASSETS_DIR "/fractal.cl", errCode);

        std::ostringstream options;
        options << "-I " << std::string(ASSETS_DIR);

        params.pprogram.build(std::vector<Device>(1, params.device), options.str().c_str());
        params.kernel = Kernel(params.pprogram, "fractal");
        // create opengl stuff
        rparams.prg = initShaders(ASSETS_DIR "/fractal.vert", ASSETS_DIR "/fractal.frag");
        rparams.tex = createTexture2D(wind_width, wind_height);
        GLuint vbo  = createBuffer(12, vertices.data(), GL_STATIC_DRAW);
        GLuint tbo  = createBuffer(8,  texcords.data(), GL_STATIC_DRAW);
        GLuint ibo;
        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices) * 6, indices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        // bind vao
        glGenVertexArrays(1, &rparams.vao);
        glBindVertexArray(rparams.vao);
        // attach vbo
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        // attach tbo
        glBindBuffer(GL_ARRAY_BUFFER, tbo);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(1);
        // attach ibo
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBindVertexArray(0);
        // create opengl texture reference using opengl texture
        params.tex = ImageGL(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, rparams.tex, &errCode);
        if (errCode != CL_SUCCESS)
            throw Exception(fmt::format( "Failed to create OpenGL texture refrence: {}", errCode));
        params.dims[0] = wind_width;
        params.dims[1] = wind_height;
        params.dims[2] = 1;
    }
    catch(Error error)
    {
        std::cout << error.what() << "(" << error.err() << ")\n"
                  << "Log:\n" << params.pprogram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(params.device) << std::endl;
        return 249;
    }

    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);

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
    return (a + b - 1) / b;
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
        cl_int res = params.queue.enqueueAcquireGLObjects(&objs, nullptr, &ev);
        ev.wait();
        if (res != CL_SUCCESS)
            throw Exception(fmt::format( "Failed acquiring GL object: {}", res));

        NDRange local(16, 16);
        NDRange global(local[0] * divup(params.dims[0], local[0]),
                       local[1] * divup(params.dims[1], local[1]));
        // set kernel arguments
        params.kernel.setArg(0, params.tex);
        params.kernel.setArg(1, (int)params.dims[0]);
        params.kernel.setArg(2, (int)params.dims[1]);
        params.kernel.setArg(3, 1.0f);
        params.kernel.setArg(4, 1.0f);
        params.kernel.setArg(5, 0.0f);
        params.kernel.setArg(6, 0.0f);
        params.kernel.setArg(7, CJULIA[2 * gJuliaSetIndex+0]);
        params.kernel.setArg(8, CJULIA[2 * gJuliaSetIndex+1]);
        params.queue.enqueueNDRangeKernel(params.kernel, cl::NullRange, global, local);
        // release opengl object
        res = params.queue.enqueueReleaseGLObjects(&objs);
        ev.wait();
        if (res!=CL_SUCCESS)
            throw Exception(fmt::format( "Failed releasing GL object: {}", res));

        params.queue.finish();
    }
    catch(Error err)
    {
        std::cout << err.what() << "(" << err.err() << ")" << std::endl;
    }
}

void renderFrame()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.2, 0.2, 0.2, 0.0);
    glEnable(GL_DEPTH_TEST);
    // bind shader
    glUseProgram(rparams.prg);
    // get uniform locations
    int mat_loc = glGetUniformLocation(rparams.prg, "matrix");
    int tex_loc = glGetUniformLocation(rparams.prg, "tex");
    // bind texture
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(tex_loc, 0);
    glBindTexture(GL_TEXTURE_2D, rparams.tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    // set project matrix
    glUniformMatrix4fv(mat_loc, 1, GL_FALSE, matrix.data());
    // now render stuff
    glBindVertexArray(rparams.vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
