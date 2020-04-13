#include "window_eglut.h"
#include "joystick_manager_linux_gamepad.h"

#include <cstring>
#include <sstream>
#include <eglut.h>
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>

EGLUTWindow* EGLUTWindow::currentWindow;

EGLUTWindow::EGLUTWindow(const std::string& title, int width, int height, GraphicsApi api) :
        WindowWithLinuxJoystick(title, width, height, api), title(title), width(width), height(height),
        graphicsApi(api) {
    eglutInitWindowSize(width, height);
    if (graphicsApi == GraphicsApi::OPENGL_ES2)
        eglutInitAPIMask(EGLUT_OPENGL_ES2_BIT);
    winId = eglutCreateWindow(title.c_str());

    eglutIdleFunc(_eglutIdleFunc);
    eglutDisplayFunc(_eglutDisplayFunc);
    eglutReshapeFunc(_eglutReshapeFunc);
    eglutMouseFunc(_eglutMouseFunc);
    eglutMouseButtonFunc(_eglutMouseButtonFunc);
    eglutMouseRawFunc(_eglutMouseRawFunc);
    eglutTouchStartFunc(_eglutTouchStartFunc);
    eglutTouchUpdateFunc(_eglutTouchUpdateFunc);
    eglutTouchEndFunc(_eglutTouchEndFunc);
    eglutKeyboardFunc(_eglutKeyboardFunc);
    eglutSpecialFunc(_eglutKeyboardSpecialFunc);
    eglutPasteFunc(_eglutPasteFunc);
    eglutFocusFunc(_eglutFocusFunc);
    eglutCloseWindowFunc(_eglutCloseWindowFunc);

    memset(pointerIds, 0xff, sizeof(pointerIds));
}

EGLUTWindow::~EGLUTWindow() {
    if (currentWindow == this)
        currentWindow = nullptr;
    if (winId != -1)
        eglutDestroyWindow(winId);
}

void EGLUTWindow::setIcon(std::string const &iconPath) {
    eglutSetWindowIcon(iconPath.c_str());
}

void EGLUTWindow::show() {
    eglutShowWindow();
    currentWindow = this;
    addWindowToGamepadManager();
}

void EGLUTWindow::close() {
    eglutDestroyWindow(currentWindow->winId);
    eglutFini();
    currentWindow->winId = -1;
}

void EGLUTWindow::pollEvents() {
    eglutPollEvents();
}

void EGLUTWindow::setCursorDisabled(bool disabled) {
    cursorDisabled = disabled;
    eglutSetMousePointerLocked(disabled ? EGLUT_POINTER_LOCKED : EGLUT_POINTER_UNLOCKED);
}

void EGLUTWindow::setFullscreen(bool fullscreen) {
    if (eglutGet(EGLUT_FULLSCREEN_MODE) != (fullscreen ? EGLUT_FULLSCREEN : EGLUT_WINDOWED))
        eglutToggleFullscreen();
}

void EGLUTWindow::swapBuffers() {
    eglutSwapBuffers();
}

void EGLUTWindow::setSwapInterval(int interval) {
    eglutSwapInterval(interval);
}

void EGLUTWindow::_eglutIdleFunc() {
    if (currentWindow == nullptr)
        return;
    currentWindow->updateGamepad();
    eglutPostRedisplay();
}

void EGLUTWindow::_eglutDisplayFunc() {
    if (currentWindow == nullptr)
        return;
    currentWindow->onDraw();
}

void EGLUTWindow::_eglutReshapeFunc(int w, int h) {
    if (currentWindow == nullptr || (currentWindow->width == w && currentWindow->height == h))
        return;
    currentWindow->onWindowSizeChanged(w, h);
    currentWindow->width = w;
    currentWindow->height = h;
}

void EGLUTWindow::_eglutMouseFunc(int x, int y) {
    if (currentWindow == nullptr)
        return;
    currentWindow->onMousePosition(x, y);
}

void EGLUTWindow::_eglutMouseRawFunc(double x, double y) {
    if (currentWindow == nullptr)
        return;
    currentWindow->onMouseRelativePosition(x, y);
}

void EGLUTWindow::_eglutMouseButtonFunc(int x, int y, int btn, int action) {
    if (currentWindow == nullptr)
        return;
    if (btn == 4 || btn == 5) {
        currentWindow->onMouseScroll(x, y, 0.0, (btn == 5 ? -1.0 : 1.0));
        return;
    }
    btn = (btn == 2 ? 3 : (btn == 3 ? 2 : btn));
    currentWindow->onMouseButton(x, y, btn, action == EGLUT_MOUSE_PRESS ? MouseButtonAction::PRESS :
                                            MouseButtonAction::RELEASE);
}

int EGLUTWindow::obtainTouchPointer(int eglutId) {
    for (int i = 0; i < sizeof(pointerIds) / sizeof(int); i++) {
        if (pointerIds[i] == eglutId)
            return i;
    }
    for (int i = 0; i < sizeof(pointerIds) / sizeof(int); i++) {
        if (pointerIds[i] == -1) {
            pointerIds[i] = eglutId;
            return i;
        }
    }
    return sizeof(pointerIds) / sizeof(int) + eglutId;
}

void EGLUTWindow::releaseTouchPointer(int ourId) {
    pointerIds[ourId] = -1;
}

void EGLUTWindow::_eglutTouchStartFunc(int id, double x, double y) {
    if (currentWindow == nullptr)
        return;
    int ourId = currentWindow->obtainTouchPointer(id);
    currentWindow->onTouchStart(ourId, x, y);
}

void EGLUTWindow::_eglutTouchUpdateFunc(int id, double x, double y) {
    if (currentWindow == nullptr)
        return;
    int ourId = currentWindow->obtainTouchPointer(id);
    currentWindow->onTouchUpdate(ourId, x, y);
}

void EGLUTWindow::_eglutTouchEndFunc(int id, double x, double y) {
    if (currentWindow == nullptr)
        return;
    int ourId = currentWindow->obtainTouchPointer(id);
    currentWindow->onTouchEnd(ourId, x, y);
    currentWindow->releaseTouchPointer(ourId);
}

void EGLUTWindow::_eglutKeyboardFunc(char str[5], int action) {
    if (currentWindow == nullptr ||
        strcmp(str, "\t") == 0 || strcmp(str, "\03") == 0 || strcmp(str, "\26") == 0 ||
        strcmp(str, "\33") == 0) // \t, copy, paste, esc
        return;
    if (action == EGLUT_KEY_PRESS || action == EGLUT_KEY_REPEAT) {
        if (str[0] == 13 && str[1] == 0)
            str[0] = 10;
        std::stringstream ss;
        ss << str;
        currentWindow->onKeyboardText(ss.str());
    }
}

mapping::KeyCode EGLUTWindow::getKeyMinecraft(int keyCode) {
    if (keyCode >= XK_A && keyCode <= XK_Z)
        return (mapping::KeyCode) (keyCode - XK_A + (int) mapping::KeyCode::A);
    if (keyCode >= XK_a && keyCode <= XK_z)
        return (mapping::KeyCode) (keyCode - XK_a + (int) mapping::KeyCode::A);
    if (keyCode >= XK_F1 && keyCode <= XK_F12)
        return (mapping::KeyCode) (keyCode - XK_F1 + (int) mapping::KeyCode::FN1);
    if (keyCode >= XK_KP_0 && keyCode <= XK_KP_9)
        return (mapping::KeyCode) (keyCode - XK_KP_0 + 96);

    switch (keyCode) {
        case XK_BackSpace: return mapping::KeyCode::BACKSPACE;
        case XK_Tab: return mapping::KeyCode::TAB;
        case XK_Return: return mapping::KeyCode::ENTER;
        case XK_Shift_L: return mapping::KeyCode::LEFT_SHIFT;
        case XK_Shift_R: return mapping::KeyCode::RIGHT_SHIFT;
        case XK_Control_L: return mapping::KeyCode::LEFT_CTRL;
        case XK_Control_R: return mapping::KeyCode::RIGHT_CTRL;
        case XK_Pause: return mapping::KeyCode::PAUSE;
        case XK_Caps_Lock: return mapping::KeyCode::CAPS_LOCK;
        case XK_Escape: return mapping::KeyCode::ESCAPE;
        case XK_Page_Up: return mapping::KeyCode::PAGE_UP;
        case XK_Page_Down: return mapping::KeyCode::PAGE_DOWN;
        case XK_End: return mapping::KeyCode::END;
        case XK_Home: return mapping::KeyCode::HOME;
        case XK_Left: return mapping::KeyCode::LEFT;
        case XK_Up: return mapping::KeyCode::UP;
        case XK_Right: return mapping::KeyCode::RIGHT;
        case XK_Down: return mapping::KeyCode::DOWN;
        case XK_Insert: return mapping::KeyCode::INSERT;
        case XK_Delete: return mapping::KeyCode::DELETE;
        case XK_Num_Lock: return mapping::KeyCode::NUM_LOCK;
        case XK_Scroll_Lock: return mapping::KeyCode::SCROLL_LOCK;
        case XK_semicolon: return mapping::KeyCode::SEMICOLON;
        case XK_equal: return mapping::KeyCode::EQUAL;
        case XK_comma: return mapping::KeyCode::COMMA;
        case XK_minus: return mapping::KeyCode::MINUS;
        case XK_period: return mapping::KeyCode::PERIOD;
        case XK_slash: return mapping::KeyCode::SLASH;
        case XK_grave: return mapping::KeyCode::GRAVE;
        case XK_bracketleft: return mapping::KeyCode::LEFT_BRACKET;
        case XK_backslash: return mapping::KeyCode::BACKSLASH;
        case XK_bracketright: return mapping::KeyCode::RIGHT_BRACKET;
        case XK_apostrophe: return mapping::KeyCode::APOSTROPHE;
    }

    if (keyCode < 256)
        return (mapping::KeyCode) keyCode;
    return mapping::KeyCode::UNKNOWN;
}

void EGLUTWindow::_eglutKeyboardSpecialFunc(int key, int action) {
    if (currentWindow == nullptr)
        return;
    if (key == 65507)
        currentWindow->modCTRL = (action != EGLUT_KEY_RELEASE);
    if (currentWindow->modCTRL && (key == 86 || key == 118) && action == EGLUT_KEY_PRESS) {
        eglutRequestPaste();
    }
    mapping::KeyCode mKey = getKeyMinecraft(key);
    KeyAction enumAction = (action == EGLUT_KEY_PRESS ? KeyAction::PRESS :
                            (action == EGLUT_KEY_REPEAT ? KeyAction::REPEAT : KeyAction::RELEASE));
    currentWindow->onKeyboard(mKey, enumAction);
}

void EGLUTWindow::_eglutPasteFunc(const char* str, int len) {
    if (currentWindow == nullptr)
        return;
    currentWindow->onPaste(std::string(str, len));
}

void EGLUTWindow::_eglutFocusFunc(int action) {
    if (currentWindow == nullptr)
        return;
    LinuxGamepadJoystickManager::instance.onWindowFocused(currentWindow, (action == EGLUT_FOCUSED));
}

void EGLUTWindow::_eglutCloseWindowFunc() {
    if (currentWindow == nullptr)
        return;
    currentWindow->onClose();
}

void EGLUTWindow::getWindowSize(int& width, int& height) const {
    width = this->width;
    height = this->height;
}

void EGLUTWindow::setClipboardText(std::string const &text) {
    eglutSetClipboardText(text.c_str());
}