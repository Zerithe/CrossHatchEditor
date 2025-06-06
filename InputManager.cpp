#include "InputManager.h"
#include "ObjLoader.h"
#include <iostream>

GLFWwindow* InputManager::m_window = nullptr;
double InputManager::m_mouseX = 0.0;
double InputManager::m_mouseY = 0.0;
bool InputManager::m_FirstMouse = true;
bool InputManager::skipPickingPass = false;
std::unordered_map<int, bool> InputManager::keyStates;

bool InputManager::m_middleMousePressed = false;
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
    return m_middleMousePressed;
}

void InputManager::setScrollCallback()
{
    glfwSetScrollCallback(m_window, scrollCallback);
}

void InputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    m_scrollDelta = static_cast<float>(yoffset);
}

void InputManager::update(Camera& camera, float deltaTime)
{
    const float cameraSpeed = camera.movementSpeed * deltaTime;
    double x, y;


    // Check middle mouse button state
    bool wasMiddleMousePressed = m_middleMousePressed;
    m_middleMousePressed = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    bool shiftPressed = isKeyPressed(GLFW_KEY_LEFT_SHIFT) || isKeyPressed(GLFW_KEY_RIGHT_SHIFT);

    // Initialize mouse position when starting to drag
    if (m_middleMousePressed && !wasMiddleMousePressed) {
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
    if (m_middleMousePressed)
    {
        getMouseMovement(&x, &y);

        if (m_isPanning) {
            // Blender-style panning: Shift + Middle Mouse
            const float panSpeed = 0.003f * m_cameraDistance;
            bx::Vec3 panRight = bx::mul(camera.right, bx::Vec3(-x * panSpeed, -x * panSpeed, -x * panSpeed));
            bx::Vec3 panUp = bx::mul(camera.up, bx::Vec3(y * panSpeed, y * panSpeed, y * panSpeed));

            camera.position = bx::add(camera.position, panRight);
            camera.position = bx::add(camera.position, panUp);
            m_cameraTarget = bx::add(m_cameraTarget, panRight);
            m_cameraTarget = bx::add(m_cameraTarget, panUp);
        }
        else if (m_isOrbiting) {
            // Blender-style orbiting: Middle Mouse
            const float orbitSpeed = 0.2f;
            camera.yaw -= x * orbitSpeed;
            camera.pitch += y * orbitSpeed;

            // Constrain pitch
            if (camera.pitch > 89.0f && camera.constrainPitch)
                camera.pitch = 89.0f;
            if (camera.pitch < -89.0f && camera.constrainPitch)
                camera.pitch = -89.0f;

            // Calculate new camera position orbiting around target
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

    // Middle Mouse Zooming
    if (m_scrollDelta != 0.0f) {
        const float zoomSpeed = 0.05f; // Smaller constant value for smoother zoom
        float zoomFactor = 1.0f - (m_scrollDelta * zoomSpeed);

        if (zoomFactor > 0.1f && zoomFactor < 10.0f) {
            m_cameraDistance *= zoomFactor;

            bx::Vec3 direction = bx::normalize(bx::sub(camera.position, m_cameraTarget));
            camera.position = bx::add(m_cameraTarget, bx::mul(direction, bx::Vec3(m_cameraDistance, m_cameraDistance, m_cameraDistance)));
        }

        m_scrollDelta = 0.0f;
    }

    // Optional WASD Controls
    if (!m_middleMousePressed) {
        if (isKeyPressed(GLFW_KEY_W))
            camera.position = bx::mad(camera.front, bx::Vec3(cameraSpeed, cameraSpeed, cameraSpeed), camera.position);
        if (isKeyPressed(GLFW_KEY_S))
            camera.position = bx::mad(camera.front, bx::Vec3(-cameraSpeed, -cameraSpeed, -cameraSpeed), camera.position);
        if (isKeyPressed(GLFW_KEY_D))
            camera.position = bx::mad(camera.right, bx::Vec3(cameraSpeed, cameraSpeed, cameraSpeed), camera.position);
        if (isKeyPressed(GLFW_KEY_A))
            camera.position = bx::mad(camera.right, bx::Vec3(-cameraSpeed, -cameraSpeed, -cameraSpeed), camera.position);
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