#include "InputManager.h"
#include "ObjLoader.h"
#include <iostream>
#include <imgui.h>

GLFWwindow* InputManager::m_window = nullptr;
double InputManager::m_mouseX = 0.0;
double InputManager::m_mouseY = 0.0;
bool InputManager::m_FirstMouse = true;
bool InputManager::skipPickingPass = false;
std::unordered_map<int, bool> InputManager::keyStates;

bool InputManager::m_rightClickMousePressed = false;
float InputManager::m_scrollDelta = 0.0f;
bx::Vec3 InputManager::m_cameraTarget = bx::Vec3(0.0f, 0.0f, 0.0f);
float InputManager::m_cameraDistance = 10.0f;
bool InputManager::m_isOrbiting = false;
bool InputManager::m_isPanning = false;

void InputManager::initialize(GLFWwindow* window)
{
    m_window = window;
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    setScrollCallback();
}

void InputManager::destroy()
{
    m_window = nullptr;
}

bool InputManager::isMiddleMousePressed()
{
    return m_rightClickMousePressed;
}

void InputManager::setScrollCallback()
{
    glfwSetScrollCallback(m_window, scrollCallback);
}

void InputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent((float)xoffset, (float)yoffset);

    if (!io.WantCaptureMouse) {
        m_scrollDelta = static_cast<float>(yoffset);
    }
    else {
        m_scrollDelta = 0.0f;
    }
}

void InputManager::update(Camera& camera, float deltaTime)
{
    const float cameraSpeed = camera.movementSpeed * deltaTime;
    double x, y;

    // Check middle mouse button state
    bool wasMiddleMousePressed = m_rightClickMousePressed;
    m_rightClickMousePressed = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool shiftPressed = isKeyPressed(GLFW_KEY_LEFT_SHIFT) || isKeyPressed(GLFW_KEY_RIGHT_SHIFT);

    // Initialize mouse position when starting to drag
    if (m_rightClickMousePressed && !wasMiddleMousePressed) {
        m_FirstMouse = true;
        glfwGetCursorPos(m_window, &m_mouseX, &m_mouseY);

        // Calculate initial camera target and distance
        m_cameraTarget = bx::mad(camera.front, bx::Vec3(m_cameraDistance, m_cameraDistance, m_cameraDistance), camera.position);
        bx::Vec3 toTarget = bx::sub(m_cameraTarget, camera.position);
        m_cameraDistance = bx::length(toTarget);

        if (shiftPressed) {
            m_isPanning = true;
            m_isOrbiting = false;
        }
        else {
            m_isOrbiting = true;
            m_isPanning = false;
        }
    }

    // Handle middle mouse
    if (m_rightClickMousePressed)
    {
        getMouseMovement(&x, &y);

        if (m_isPanning) {
            const float panSpeed = 0.003f * m_cameraDistance;
            bx::Vec3 panRight = bx::mul(camera.right, bx::Vec3(x * panSpeed, x * panSpeed, x * panSpeed));
            bx::Vec3 panUp = bx::mul(camera.up, bx::Vec3(-y * panSpeed, -y * panSpeed, -y * panSpeed));

            camera.position = bx::add(camera.position, panRight);
            camera.position = bx::add(camera.position, panUp);
            m_cameraTarget = bx::add(m_cameraTarget, panRight);
            m_cameraTarget = bx::add(m_cameraTarget, panUp);
        }
        else if (m_isOrbiting) {
            const float orbitSpeed = 0.2f;
            camera.yaw -= x * orbitSpeed;
            camera.pitch += y * orbitSpeed;

            if (camera.pitch > 89.0f && camera.constrainPitch)
                camera.pitch = 89.0f;
            if (camera.pitch < -89.0f && camera.constrainPitch)
                camera.pitch = -89.0f;

            float yawRad = bx::toRad(camera.yaw);
            float pitchRad = bx::toRad(camera.pitch);

            bx::Vec3 direction = bx::Vec3(
                cos(yawRad) * cos(pitchRad),
                sin(pitchRad),
                sin(yawRad) * cos(pitchRad)
            );

            camera.front = bx::normalize(direction);
            camera.position = bx::sub(m_cameraTarget, bx::mul(camera.front, bx::Vec3(m_cameraDistance, m_cameraDistance, m_cameraDistance)));

            camera.right = bx::normalize(bx::cross(camera.front, bx::Vec3(0.0f, 1.0f, 0.0f)));
            camera.up = bx::normalize(bx::cross(camera.right, camera.front));
        }
    }
    else
    {
        // Reset states when middle mouse is released
        m_isOrbiting = false;
        m_isPanning = false;
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    // Scroll Wheel Zooming
    if (m_scrollDelta != 0.0f) {
        const float zoomSpeed = 2.0f;
        float zoomAmount = m_scrollDelta * zoomSpeed;

        bx::Vec3 movement = bx::mul(camera.front, bx::Vec3(zoomAmount, zoomAmount, zoomAmount));
        camera.position = bx::add(camera.position, movement);

        if (!m_rightClickMousePressed) {
            m_cameraTarget = bx::mad(camera.front, bx::Vec3(m_cameraDistance, m_cameraDistance, m_cameraDistance), camera.position);
        }

        m_scrollDelta = 0.0f;
    }

    // Optional WASD Controls
    if (!m_rightClickMousePressed) {
        if (isKeyPressed(GLFW_KEY_W))
            camera.position = bx::mad(camera.front, bx::Vec3(cameraSpeed, cameraSpeed, cameraSpeed), camera.position);
        if (isKeyPressed(GLFW_KEY_S))
            camera.position = bx::mad(camera.front, bx::Vec3(-cameraSpeed, -cameraSpeed, -cameraSpeed), camera.position);
        if (isKeyPressed(GLFW_KEY_D))
            camera.position = bx::mad(camera.right, bx::Vec3(-cameraSpeed, -cameraSpeed, -cameraSpeed), camera.position);
        if (isKeyPressed(GLFW_KEY_A))
            camera.position = bx::mad(camera.right, bx::Vec3(cameraSpeed, cameraSpeed, cameraSpeed), camera.position);
        if (isKeyPressed(GLFW_KEY_SPACE))
            camera.position = bx::mad(camera.up, bx::Vec3(cameraSpeed, cameraSpeed, cameraSpeed), camera.position);
        if (isKeyPressed(GLFW_KEY_LEFT_CONTROL))
            camera.position = bx::mad(camera.up, bx::Vec3(-cameraSpeed, -cameraSpeed, -cameraSpeed), camera.position);
    }
}

bool InputManager::isKeyPressed(int key)
{
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

bool InputManager::isKeyToggled(int key)
{
    bool isPressed = glfwGetKey(m_window, key) == GLFW_PRESS;
    if (isPressed && !keyStates[key])
    {
        keyStates[key] = true;
        return true;
    }
    else if (!isPressed)
    {
        keyStates[key] = false;
    }
    return false;
}

bool InputManager::isMouseClicked(int key)
{
    bool isPressed = glfwGetMouseButton(m_window, key) == GLFW_PRESS;
    if (isPressed && !keyStates[key])
    {
        keyStates[key] = true;
        return true;
    }
    else if (!isPressed)
    {
        keyStates[key] = false;
    }
    return false;
}

void InputManager::getMouseMovement(double* x, double* y)
{
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);

    if (m_FirstMouse)
    {
        m_mouseX = xpos;
        m_mouseY = ypos;
        m_FirstMouse = false;
    }

    *x = xpos - m_mouseX;
    *y = m_mouseY - ypos;

    m_mouseX = xpos;
    m_mouseY = ypos;
}

double InputManager::getMouseX() {
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);
    return xpos;
}

double InputManager::getMouseY() {
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);
    return ypos;
}
