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
void InputManager::initialize(GLFWwindow* window)
{
	m_window = window;
	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void InputManager::destroy()
{
	m_window = nullptr;
}

bool InputManager::isMiddleMousePressed()
{
	return m_middleMousePressed;
}

void InputManager::update(Camera& camera, float deltaTime)
{
	const float cameraSpeed = camera.movementSpeed * deltaTime;
    double x, y;

    
	bool wasMiddleMousePressed = m_middleMousePressed;
	m_middleMousePressed = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

	if (m_middleMousePressed && !wasMiddleMousePressed) {
		m_FirstMouse = true;
		
		glfwGetCursorPos(m_window, &m_mouseX, &m_mouseY);
	}
    if (m_middleMousePressed)
    {

        if (isKeyPressed(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(m_window, true);
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
        if (isKeyPressed(GLFW_KEY_LEFT_SHIFT))
            camera.position = bx::mad(camera.up, bx::Vec3(-cameraSpeed, -cameraSpeed, -cameraSpeed), camera.position);

        getMouseMovement(&x, &y);
		camera.yaw -= x * camera.mouseSensitivity;
		camera.pitch += y * camera.mouseSensitivity;

        if (camera.pitch > 89.0f && camera.constrainPitch)
            camera.pitch = 89.0f;
        if (camera.pitch < -89.0f && camera.constrainPitch)
            camera.pitch = -89.0f;

        bx::Vec3 direction = bx::Vec3(cos(bx::toRad(camera.yaw)) * cos(bx::toRad(camera.pitch)), sin(bx::toRad(camera.pitch)), sin(bx::toRad(camera.yaw)) * cos(bx::toRad(camera.pitch)));
        camera.front = bx::normalize(direction);

        camera.right = bx::normalize(bx::cross(camera.front, bx::Vec3(0.0f, 1.0f, 0.0f)));
        camera.up = bx::normalize(bx::cross(camera.right, camera.front));
    }
    else
    {
        // Re-enable the cursor when the middle mouse button is not pressed
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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