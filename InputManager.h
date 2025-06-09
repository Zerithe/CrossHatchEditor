#include <GLFW/glfw3.h>
#include <bx/math.h>
#include "Camera.h"
#include <unordered_map>

class InputManager
{
public:
	static void initialize(GLFWwindow* window);
	static void destroy();

	static void update(Camera& camera, float deltaTime);
	static double getMouseX();
	static double getMouseY();
	static bool isKeyPressed(int key);
	static bool isKeyToggled(int key);
	static bool isMouseClicked(int key);
	static void getMouseMovement(double* x, double* y);

	static bool getCursorDisabled() { return isCursorDisabled; }
	static bool getSkipPickingPass() { return skipPickingPass; }
	static void toggleSkipPickingPass() { skipPickingPass = !skipPickingPass; }

	static bool isMiddleMousePressed();
	static void setScrollCallback();
	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

private:
	InputManager() = default;
	~InputManager() = default;

	static GLFWwindow* m_window;

	static double m_mouseX;
	static double m_mouseY;
	static bool m_FirstMouse;
	static bool isCursorDisabled;
	static bool skipPickingPass;
	static std::unordered_map<int, bool> keyStates;

	static bool m_rightClickMousePressed;
	static float m_scrollDelta;

	static bx::Vec3 m_cameraTarget;  
	static float m_cameraDistance;
	static bool m_isOrbiting;
	static bool m_isPanning;
};