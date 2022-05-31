
#include "common/Board.h"
#include "common/OpenClTypes.h"

#include "assets/Actor.h"

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

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <random>
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

static int boardWidth = 0;
static int boardHeight = 0;

struct process_params
{
    Device device;
    CommandQueue queue;
    Program boardProgram;
    Kernel boardKernel;
    Program actorProgram;
    Kernel actorKernel;
    ImageGL tex;
    Buffer cells;
    int2 boardSize{};
    Buffer actors;
    int actorSize = 0;
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
    }
}

static void glfw_framebuffer_size_callback(GLFWwindow* wind, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processTimeStep(int generation);
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

    boardWidth  = mode->width - 100;
    boardHeight = mode->height - 100;
    float2 center{boardWidth / 2.f, boardHeight / 2.f};

    std::mt19937_64 rng;
    uint64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::seed_seq ss{uint32_t(timeSeed & 0xffffffff), uint32_t(timeSeed>>32)};
    rng.seed(ss);
    std::uniform_real_distribution<double> unif(0, 1);

    Board board(boardWidth, boardHeight);
    for (int y = 0; y < boardHeight; ++y)
        for (int x = 0; x < boardWidth; ++x)
        {
            const double divX = min(x, boardWidth - x);
            const double divY = min(y, boardHeight - y);
            const double div = 1 / divX + 1 / divY;
            board(x, y).solid = div > .1;
            board(x, y).trail = 0;
        }

    std::vector<Actor> actors(1000, Actor{{0, 0}, 0, 0, false});


    std::normal_distribution<> targetSpeedDistribution{.5, .05};
    for (Actor &a : actors)
    {
        a.alive = true;
        const double r = boardHeight / 2.1 * sqrt(unif(rng));
        const double theta = unif(rng) * 2 * M_PI;
        a.pos = {static_cast<float>(center.x + r * cos(theta)), static_cast<float>(center.y + r * sin(theta))};
        a.speed = 0;
        a.targetSpeed = targetSpeedDistribution(rng);
        a.direction = unif(rng) * 2 * M_PI;
    }

    cout << fmt::format("C++ - sizeof(Cell) = {}, sizeof(Actor) = {}", sizeof(Cell), sizeof(Actor))  << endl;

    GLFWwindow* window;

    glfwSetErrorCallback(glfw_error_callback);

    window = glfwCreateWindow(boardWidth, boardHeight, "Test", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 254;
    }

    glfwMakeContextCurrent(window);

    if(!gladLoadGL())
        throw Exception("gladLoadGL failed!");
    //cout << fmt::format("OpenGL {}.{}", GLVersion.major, GLVersion.minor) << endl;

    cl_int errCode;

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
    params.boardProgram = getProgram(context, ASSETS_DIR"/board.cl", errCode);
    params.actorProgram = getProgram(context, ASSETS_DIR"/Actor.cl", errCode);

    std::ostringstream options;
    options << "-I " << std::string(ASSETS_DIR);

    try
    {
        params.boardProgram.build(std::vector<Device>(1, params.device), options.str().c_str());
    }
    catch(Error error)
    {
        std::cout << error.what() << "(" << error.err() << ")\n"
                  << "Log:\n" << params.boardProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(params.device) << std::endl;
        return 249;
    }

    try
    {
        params.actorProgram.build(std::vector<Device>(1, params.device), options.str().c_str());
    }
    catch(Error error)
    {
        std::cout << error.what() << "(" << error.err() << ")\n"
                  << "Log:\n" << params.actorProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(params.device) << std::endl;
        return 249;
    }

    params.boardKernel = Kernel(params.boardProgram, "board");
    params.actorKernel = Kernel(params.actorProgram, "actor");
    // create opengl stuff
    rparams.prg = initShaders(ASSETS_DIR "/board.vert", ASSETS_DIR "/board.frag");
    rparams.tex = createTexture2D(boardWidth, boardHeight);
    GLuint vbo  = createBuffer(12, vertices.data(), GL_STATIC_DRAW);
    GLuint tbo  = createBuffer(8,  texcords.data(), GL_STATIC_DRAW);
    GLuint ibo;
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);
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
    params.cells = Buffer(context, CL_MEM_READ_WRITE, board.dataSize(), nullptr, &errCode);
    if (errCode != CL_SUCCESS)
        throw Exception(fmt::format( "Failed to create OpenGL texture refrence: {}", errCode));
    params.actors = Buffer(context, CL_MEM_READ_WRITE, sizeof(Actor) * actors.size(), nullptr, &errCode);
    if (errCode != CL_SUCCESS)
        throw Exception(fmt::format( "Failed to create OpenGL texture refrence: {}", errCode));
    params.actorSize = actors.size();

    params.boardSize.x = boardWidth;
    params.boardSize.y = boardHeight;

    params.queue.enqueueWriteBuffer(params.cells, true, 0, board.dataSize(), board.cells().data());
    params.queue.enqueueWriteBuffer(params.actors, true, 0, sizeof(Actor) * actors.size(), actors.data());
    params.queue.finish();
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);

    int generation = 0;
    while (!glfwWindowShouldClose(window))
    {
        for (int i = 0; i < 10; ++i)
        {
            // process call
            processTimeStep(generation);
            ++generation;
        }
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

void processTimeStep(int generation)
{
    cl::Event ev;
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
    NDRange global(local[0] * divup(params.boardSize.x, local[0]),
                   local[1] * divup(params.boardSize.y, local[1]));
    // set kernel arguments
    params.boardKernel.setArg(0, params.tex);
    params.boardKernel.setArg(1, params.cells);
    params.boardKernel.setArg(2, params.boardSize);
    params.queue.enqueueNDRangeKernel(params.boardKernel, cl::NullRange, global, local);

    local = NDRange(16);
    global = NDRange(local[0] * divup(params.actorSize, local[0]));
    params.actorKernel.setArg(0, params.cells);
    params.actorKernel.setArg(1, params.boardSize);
    params.actorKernel.setArg(2, params.actors);
    params.actorKernel.setArg(3, params.actorSize);
    params.actorKernel.setArg(4, generation);
    params.queue.enqueueNDRangeKernel(params.actorKernel, cl::NullRange, global, local);
    // release opengl object
    res = params.queue.enqueueReleaseGLObjects(&objs);

    ev.wait();
    if (res!=CL_SUCCESS)
        throw Exception(fmt::format( "Failed releasing GL object: {}", res));

    params.queue.finish();
}

void renderFrame()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.2, 0.2, 0.2, 0.0);
    glEnable(GL_DEPTH_TEST);
    // bind shader
    glUseProgram(rparams.prg);
    // get uniform locations
    int mat_loc = glGetUniformLocation(rparams.prg, "pos");
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
