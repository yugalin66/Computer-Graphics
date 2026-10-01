#include "OpenGLWindow.hpp"

#include "Model/TextureFactory.hpp"
#include "OpenGL/OpenGLException.hpp"
#include "Utils/Compilers.hpp"
#include "Utils/Global.hpp"
#include "Utils/StringFormat/StringFormat.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/mat4x4.hpp"

#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "tiny_obj_loader.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace Detail
{

bool compileShaders(OpenGL::OpenGLShaderProgram &program,
                    const char *vertexShaderFile,
                    const char *fragmentShaderFile = nullptr,
                    const char *geometryShaderFile = nullptr);
void frameBufferSizeCallback(GLFWwindow *window, int width, int height);

bool compileShaders(OpenGL::OpenGLShaderProgram &program,
                    const char *vertexShaderFile,
                    const char *fragmentShaderFile,
                    const char *geometryShaderFile)
{
    if (!program.addShaderFromFile(OpenGL::OpenGLShader::Type::Vertex,
                                   vertexShaderFile))
    {
        return false;
    }

    if (fragmentShaderFile &&

        !program.addShaderFromFile(OpenGL::OpenGLShader::Type::Fragment,
                                   fragmentShaderFile))
    {
        return false;
    }

    if (geometryShaderFile &&
        !program.addShaderFromFile(OpenGL::OpenGLShader::Type::Geometry,
                                   geometryShaderFile))
    {
        return false;
    }

    program.link();
    if (!program.linkStatus())
    {
        return false;
    }

    return true;
}

void frameBufferSizeCallback(GLFWwindow *window, int width, int height)
{
    if (window)
    {
        auto *size = static_cast<glm::ivec2 *>(glfwGetWindowUserPointer(window));
        if (size) *size = glm::ivec2{width, height};
    }
    glViewport(0, 0, width, height);
}

} // namespace Detail

OpenGLWindow::OpenGLWindow(glm::ivec2 windowSize, std::string title,
                           glm::ivec2 openglVersion)
    : window_{nullptr}, size_{windowSize}, title_{title},
      version_{openglVersion}, models_{}, renderMode_{RenderMode::Fill},
      backgroundColor_{0}, lookAt_{0}, cameraPosition_{lookAt_ + glm::vec3{8}}
{
    const char *demo = std::getenv("HW1_DEMO_DIR");
    if (demo) demoDirectory_ = demo;
    create();
}

OpenGLWindow::~OpenGLWindow() { destroy(); }

bool OpenGLWindow::addModel(const char *modelSource, const char *textureSource,
                            OpenGL::OpenGLShaderProgram &program)
{
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string errorMessage;

    auto success =
        tinyobj::LoadObj(shapes, materials, errorMessage, modelSource);

    if (!success)
    {
        std::cerr << "[Error]" << errorMessage.c_str();

        return false;
    }

    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> textureCoordinates;
    std::vector<unsigned int> indices;

    for (auto &shape : shapes)
    {
        const auto vertexOffset = static_cast<unsigned int>(positions.size() / 3);
        positions.insert(positions.end(), shape.mesh.positions.begin(),
                         shape.mesh.positions.end());
        if (!shape.mesh.normals.empty())
        {
            normals.insert(normals.end(), shape.mesh.normals.begin(),
                           shape.mesh.normals.end());
        }

        if (!shape.mesh.texcoords.empty())
        {
            textureCoordinates.insert(textureCoordinates.end(),
                                      shape.mesh.texcoords.begin(),
                                      shape.mesh.texcoords.end());
        }

        for (auto index : shape.mesh.indices) indices.push_back(vertexOffset + index);
    }

    if (positions.empty() || indices.empty()) return false;
    // Center and fit arbitrary OBJ units into a five-unit bounding box.
    glm::vec3 minimum{std::numeric_limits<float>::max()};
    glm::vec3 maximum{std::numeric_limits<float>::lowest()};
    for (size_t i = 0; i < positions.size(); i += 3)
        for (size_t axis = 0; axis < 3; ++axis)
        {
            minimum[axis] = std::min(minimum[axis], positions[i + axis]);
            maximum[axis] = std::max(maximum[axis], positions[i + axis]);
        }
    const glm::vec3 center = (minimum + maximum) * 0.5f;
    const glm::vec3 extent = maximum - minimum;
    const float largest = std::max(extent.x, std::max(extent.y, extent.z));
    const float scale = largest > 0 ? 5.0f / largest : 1.0f;
    for (size_t i = 0; i < positions.size(); i += 3)
        for (size_t axis = 0; axis < 3; ++axis)
            positions[i + axis] = (positions[i + axis] - center[axis]) * scale;
    if (normals.size() != positions.size()) normals.assign(positions.size(), 0.0f);
    if (textureCoordinates.size() != positions.size() / 3 * 2)
        textureCoordinates.assign(positions.size() / 3 * 2, 0.0f);

    std::unique_ptr<OpenGL::OpenGLTexture> texture;
    std::unique_ptr<Model::Mesh> mesh;

    if (textureSource)
    {
        texture = Model::TextureFactory::loadFromFile(textureSource);
        mesh.reset(new Model::Mesh{positions, normals, textureCoordinates,
                                   indices, program, texture.get()});

        textures.push_back(std::move(texture));
    }
    else
    {
        mesh.reset(new Model::Mesh{positions, normals, textureCoordinates,
                                   indices, program});
    }

    models_.push_back(std::move(mesh));

    return true;
}

OpenGL::OpenGLShaderProgram *
OpenGLWindow::addShader(const char *vertexShaderSource,
                        const char *fragmentShaderSource,
                        const char *geometryShaderSource)
{
    std::unique_ptr<OpenGL::OpenGLShaderProgram> program{
        new OpenGL::OpenGLShaderProgram{}};
    if (!Detail::compileShaders(*program, vertexShaderSource,
                                fragmentShaderSource, geometryShaderSource))
    {
        return nullptr;
    }
    shaders_.push_back(std::move(program));

    return shaders_.back().get();
}

float OpenGLWindow::aspectRatio() const noexcept
{
    return static_cast<float>(width()) / static_cast<float>(height() > 0 ? height() : 1);
}

void OpenGLWindow::create()
{
    if (!initializeOpenGL())
    {
        throw OpenGL::OpenGLException{"Failed to initialize OpenGL"};
    }

    if (!createWindow())
    {
        glfwTerminate();
        throw OpenGL::OpenGLException{"Failed to Create GLFW window"};
    }

    if (!initializeGLAD())
    {
        glfwTerminate();
        throw OpenGL::OpenGLException{"Failed to initialize GLAD"};
    }

    glViewport(0, 0, size_.x, size_.y);
    initializeImgui();

    glEnable(GL_DEPTH_TEST);
}

void OpenGLWindow::clearColor()
{
    glClearColor(backgroundColor_.x, backgroundColor_.y, backgroundColor_.z,
                 backgroundColor_.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

bool OpenGLWindow::createWindow()
{
    window_ =
        glfwCreateWindow(width(), height(), title_.c_str(), nullptr, nullptr);
    if (!window_)
    {
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, &size_);
    glfwGetFramebufferSize(window_, &size_.x, &size_.y);
    glfwSetFramebufferSizeCallback(window_, Detail::frameBufferSizeCallback);
    glfwSwapInterval(1);

    return true;
}

void OpenGLWindow::destroy()
{
    for (auto &model : models_)
    {
        model.reset(nullptr);
    }
    models_.clear();

    for (auto &texture : textures)
    {
        texture.reset(nullptr);
    }
    textures.clear();

    shaders_.clear();

    destroyImgui();
    destroyOpenGL();
}

void OpenGLWindow::destroyImgui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void OpenGLWindow::destroyOpenGL()
{
    glfwDestroyWindow(window_);
    glfwTerminate();
}

int OpenGLWindow::height() const noexcept { return size_.y; }

bool OpenGLWindow::initializeGLAD()
{
    return gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
}

void OpenGLWindow::initializeImgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    PROGRAM_MAYBE_UNUSED(io)

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(
        (std::string("#version ") + std::to_string(version_.x) +
         std::to_string(version_.y) + "0").c_str());
}

bool OpenGLWindow::initializeOpenGL()
{
    if (!glfwInit())
    {
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, version_[0]);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, version_[1]);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    return true;
}

void OpenGLWindow::processInput() { shouldExit(); }

void OpenGLWindow::shouldExit()
{
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window_, true);
    }
}

void OpenGLWindow::startRender() { windowRenderLoop(); }

int OpenGLWindow::width() const noexcept { return size_.x; }

void OpenGLWindow::windowImguiGeneralSetting()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(340, 320), ImGuiCond_Once);
    ImGui::Begin("Setting");
    ImGui::PushItemWidth(155);
    ImGui::ColorEdit4("Background color", glm::value_ptr(backgroundColor_));
    ImGui::SliderFloat3("Camera Position", glm::value_ptr(cameraPosition_),
                        -20.0f, 20.0f);
    ImGui::SliderFloat3("Look At", glm::value_ptr(lookAt_), -20.0f, 20.0f);

    const char *items[] = {"Line", "Fill"};
    int current_item = renderMode_;
    ImGui::Combo("Render mode", &current_item, items, IM_ARRAYSIZE(items));

    if (current_item != renderMode_)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE + current_item);
        renderMode_ = static_cast<RenderMode>(current_item);
    }

    ImGui::SliderFloat("Field of view", &fieldOfView_, 20.0f, 90.0f);
    ImGui::Checkbox("Auto orbit camera", &autoOrbit_);
    if (models_.size() == 4)
    {
        const char *modelNames[] = {"Utah teapot", "UV cube (original)"};
        const char *textureNames[] = {"UV reference", "Checker (original)"};
        ImGui::Combo("Model", &modelChoice_, modelNames, 2);
        ImGui::Combo("Texture", &textureChoice_, textureNames, 2);
    }
    if (ImGui::Button("Reset view"))
    {
        cameraPosition_ = glm::vec3{8}; lookAt_ = glm::vec3{0};
        fieldOfView_ = 45.0f; backgroundColor_ = glm::vec4{0};
        autoOrbit_ = false; renderMode_ = RenderMode::Fill;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    ImGui::TextUnformatted("ESC: exit");
    ImGui::PopItemWidth();
    ImGui::End();
    if (!demoDirectory_.empty())
    {
        ImGui::SetNextWindowPos(ImVec2(10, 450));
        ImGui::SetNextWindowSize(ImVec2(780, 140));
        ImGui::Begin("Demo guide", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        const int stage = demoFrame_ / 120;
        const char *labels[] = {"1. Run the program and render the textured teapot",
            "2. Camera position and look-at controls", "3. Background and field of view",
            "4. Wireframe rendering", "5. Bonus: change texture", "6. Bonus: change model"};
        ImGui::TextUnformatted(labels[stage < 6 ? stage : 5]);
        ImGui::TextUnformatted("From sample_code: ./run.sh");
        ImGui::TextUnformatted("Build: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release");
        ImGui::TextUnformatted("       cmake --build build -j4");
        ImGui::TextUnformatted("Settings above are interactive. Demo changes them automatically.");
        ImGui::End();
    }
}

void OpenGLWindow::windowRenderLateUpdate() {}

void OpenGLWindow::windowRenderImguiUpdate()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    windowImguiGeneralSetting();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void OpenGLWindow::windowRenderLoop()
{
    while (!(glfwWindowShouldClose(window_)))
    {
        processInput();
        if (!demoDirectory_.empty())
        {
            const int stage = demoFrame_ / 120;
            const float t = static_cast<float>(demoFrame_) / 30.0f;
            cameraPosition_ = stage == 0 ? glm::vec3{8} : glm::vec3{11.0f * std::cos(t * 0.4f), 6.0f, 11.0f * std::sin(t * 0.4f)};
            lookAt_ = stage == 1 ? glm::vec3{0, 0.5f * std::sin(t), 0} : glm::vec3{0};
            backgroundColor_ = stage >= 2 ? glm::vec4{0.08f, 0.12f, 0.18f, 1} : glm::vec4{0};
            fieldOfView_ = stage == 2 ? 35.0f + 10.0f * std::sin(t) : 45.0f;
            renderMode_ = stage == 3 ? RenderMode::Line : RenderMode::Fill;
            glPolygonMode(GL_FRONT_AND_BACK, renderMode_ == RenderMode::Line ? GL_LINE : GL_FILL);
            textureChoice_ = stage >= 4 ? 1 : 0;
            modelChoice_ = stage >= 5 ? 1 : 0;
        }
        clearColor();

        windowRenderUpdate();
        windowRenderLateUpdate();

        windowRenderImguiUpdate();

        if (!demoDirectory_.empty())
        {
            std::vector<unsigned char> pixels(static_cast<size_t>(width() * height() * 3));
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, width(), height(), GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            std::ostringstream filename;
            filename << demoDirectory_ << "/frame" << std::setw(4) << std::setfill('0') << demoFrame_ << ".ppm";
            std::ofstream out(filename.str(), std::ios::binary);
            out << "P6\n" << width() << " " << height() << "\n255\n";
            for (int y = height() - 1; y >= 0; --y)
                out.write(reinterpret_cast<const char *>(pixels.data() + y * width() * 3), width() * 3);
            if (!out) throw std::runtime_error("Cannot write demo frame");
            const auto error = glGetError();
            if (error != GL_NO_ERROR) throw std::runtime_error("OpenGL error during demo: " + std::to_string(error));
            if (++demoFrame_ >= 720) glfwSetWindowShouldClose(window_, true);
        }
        glfwSwapBuffers(window_);
        glfwPollEvents();
    }
}

void OpenGLWindow::windowRenderUpdate()
{
    PRAGMA_WARNING_PUSH
    PRAGMA_WARNING_DISABLE_CONSTANTCONDITIONAL
    if (autoOrbit_ && demoDirectory_.empty())
    {
        const float t = static_cast<float>(glfwGetTime());
        cameraPosition_ = lookAt_ + glm::vec3{11.0f * std::cos(t * 0.4f), 6.0f, 11.0f * std::sin(t * 0.4f)};
    }
    glm::vec3 direction = lookAt_ - cameraPosition_;
    if (glm::length(direction) < 0.001f) direction = glm::vec3{0, 0, -1};
    const glm::vec3 up = std::abs(glm::normalize(direction).y) > 0.99f
        ? glm::vec3{0, 0, 1} : glm::vec3{0, 1, 0};
    glm::mat4 view = glm::lookAt(cameraPosition_, cameraPosition_ + direction, up) *
                     glm::mat4(1);
    PRAGMA_WARNING_POP

    // Reserve the left side for controls and the bottom for the demo guide.
    const int left = width() > 500 ? 350 : 0;
    const int bottom = demoDirectory_.empty() ? 0 : 150;
    const int sceneWidth = std::max(1, width() - left);
    const int sceneHeight = std::max(1, height() - bottom);
    glViewport(left, bottom, sceneWidth, sceneHeight);
    glm::mat4 projection{
        glm::perspective(glm::radians(fieldOfView_),
                         static_cast<float>(sceneWidth) / static_cast<float>(sceneHeight), 0.1f, 100.0f)};

    const size_t selected = models_.size() == 4
        ? static_cast<size_t>(modelChoice_ * 2 + textureChoice_) : 0;
    if (selected < models_.size()) models_[selected]->draw(view, projection);
}
