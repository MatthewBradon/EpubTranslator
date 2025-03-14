#include "main.h"

void fileDropCallback(GLFWwindow* window, int count, const char** paths) {
    // Retrieve the GUI instance stored in window user pointer
    GUI* gui = static_cast<GUI*>(glfwGetWindowUserPointer(window));
    if (gui) {
        gui->handleFileDrop(count, paths);
    }
}




int main() {
    try{
        // All the couts are redirected to captureOutput so we can use it in our GUI
        std::streambuf* originalBuffer = std::cout.rdbuf();
        std::ostringstream captureOutput;
        // DEBUG: Comment out this line if you want the couts to be in the terminal for debugging purposes
        std::cout.rdbuf(captureOutput.rdbuf());
        
        // Setup window
        if (!glfwInit())
            return 1;

    // Decide GL+GLSL versions
    #if __APPLE__
        // GL 3.2 + GLSL 150
        const char *glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);		   // Required on Mac
    #else
        // GL 3.0 + GLSL 130
        const char *glsl_version = "#version 130";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    #endif

        // Create window with graphics context
        GLFWwindow *window = glfwCreateWindow(720, 420, "Book Translator", NULL, NULL);
        if (window == NULL)
            return 1;
        glfwSetWindowSizeLimits(window, 720, 420, GLFW_DONT_CARE, GLFW_DONT_CARE);
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Enable vsync

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cout << "Failed to initialize GLAD" << "\n";
            return -1;
        }

        int screen_width, screen_height;
        glfwGetFramebufferSize(window, &screen_width, &screen_height);
        glViewport(0, 0, screen_width, screen_height);

        GUI gui;

        gui.init(window, glsl_version);

        // Store reference to GUI instance in the window user pointer
        glfwSetWindowUserPointer(window, &gui);
        glfwSetDropCallback(window, fileDropCallback);

        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);

            gui.newFrame();
            gui.update(captureOutput);
            gui.render();

            glfwSwapBuffers(window);
        }

        gui.shutdown();
        std::cout.rdbuf(originalBuffer); // Restore original std::cout buffer
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return -1;
    }

    return 0;
}

#ifdef _WIN32
#include <windows.h>

// Entry point for a Windows GUI application
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main();
}

#endif