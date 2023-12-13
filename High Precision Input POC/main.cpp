#include <windows.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <thread> // use windows threads? cuz I have windows defines? HAHA 

std::chrono::high_resolution_clock::time_point TheBeginningOfTime;
std::chrono::high_resolution_clock::time_point LastShotTime;

// adjust it for yourself, I use 20k dpi, this POC utilizes raw input!
#define sensitivity 0.0001
#define simulated_tickrate 16
#define MeasureTimeDeltaMS(A) std::chrono::duration_cast<std::chrono::milliseconds>(A).count()

struct MouseEvent {
    LONG horizontal;
    LONG vertical;
    std::chrono::high_resolution_clock::time_point timestamp;
    // 0 - none, 1 - changed to down, 2 - changed to up
    INT leftMouseState;

    void printData()
    {
        std::cout
            << "H: " << horizontal << std::endl
            << "V: " << vertical << std::endl
            << "L: " << leftMouseState << std::endl
            << "T: " << std::chrono::duration_cast<std::chrono::microseconds>(timestamp - TheBeginningOfTime).count() << std::endl
            << std::endl;
    }
};

// is there any reason for this being a DWORD WINAPI? no.
DWORD WINAPI TickHandler();
DWORD WINAPI DataCollector();
std::vector<MouseEvent> MouseEvents;
std::mutex mouseEventsMutex;

struct BasedPlayer {
    double yaw = 0.f;
    double pitch = 0.f;
    BOOL leftMouseDown = FALSE;

    void normalizeViewAngles()
    {
        // is this a good way of doing it? no. Is it a funny meme? yes.
        while (this->yaw > 180) this->yaw -= 360;
        while (this->yaw < -180) this->yaw += 360;

        this->pitch = std::clamp(pitch, -89., 89.);
    }
};

BasedPlayer localPlayer;

int main()
{
    TheBeginningOfTime = std::chrono::high_resolution_clock::now();
    LastShotTime = TheBeginningOfTime;

    std::thread a1(TickHandler); 
    std::thread a2(DataCollector); 

    // will never happen :) 
    // you can put an infinite loop here instead
    a1.join(); a2.join();

    // or here, yes. fabulous.
    for (;;);

    return -1;
}

std::string DataMuncher(std::vector<MouseEvent>& events)
{
    std::stringstream output;

    for (const auto& me : events)
    {
        if (me.leftMouseState == 1)
        {
            localPlayer.leftMouseDown = TRUE;
        }
        if (me.leftMouseState == 2)
        {
            localPlayer.leftMouseDown = FALSE;
        }

        if (localPlayer.leftMouseDown &&
            // shot every 100ms, cuz ak47 is 600rpm
            MeasureTimeDeltaMS(me.timestamp - LastShotTime) >= 100)
        {
            localPlayer.normalizeViewAngles();

            output << "Shot! Yaw: " << localPlayer.yaw
                << " Pitch: " << localPlayer.pitch
                << " Time: " << MeasureTimeDeltaMS(me.timestamp - TheBeginningOfTime) << " ms since launch"
            << std::endl;

            LastShotTime = me.timestamp;
        }

        localPlayer.yaw += me.horizontal * sensitivity;
        localPlayer.pitch += me.vertical * sensitivity;
    }

    // # anti untrusted
    localPlayer.normalizeViewAngles();

    // in case we got 0 mouse events, still do the tick/frame based update, so gun fires and stuff
    // this can happen if you're holding mouse1 while having your mouse in air, or if you're not overdosing caffeine....
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    auto time_delta = MeasureTimeDeltaMS(now - LastShotTime);

    if (localPlayer.leftMouseDown && time_delta >= 100)
    {
        output << "Shot! Yaw: " << localPlayer.yaw
            << " Pitch: " << localPlayer.pitch
            << " Time: " << time_delta
            << std::endl;

        LastShotTime = now;
    }

    return output.str();
}

// cs2 subtick wannabe
DWORD WINAPI TickHandler()
{
    std::vector<MouseEvent> MouseEventsCopy;
    std::chrono::high_resolution_clock::time_point last = std::chrono::high_resolution_clock::now();

    for (;;)
    {
        // Get current time
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        auto delta = now - last;
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(delta).count();

        // wait for next tick/frame (which isn't real in this POC)
        if (ms < (1./simulated_tickrate) * 1000 * 1000)
            continue;

        last = now;

        MouseEventsCopy.clear();
        {
            // Lock the mutex before accessing shared data
            std::lock_guard<std::mutex> guard(mouseEventsMutex);
            MouseEventsCopy = MouseEvents;
            MouseEvents.clear();
        }

        std::stringstream output;

        // this part uses naive sleep, so it's a lil inaccurate but who cares, so is subtick in cs2
        output << "Just a reminder that this proof of concept creates a transparent fullscreen window, use alt+f4 if you need to exit" << std::endl;
        output << "Received " << MouseEventsCopy.size() << " since last tick." << std::endl;

        // also updates yaw and pitch, so print these first
        std::string shot_data = DataMuncher(MouseEventsCopy);

        output << "Current YAW: " << localPlayer.yaw << std::endl;
        output << "Current PITCH: " << localPlayer.pitch << std::endl;

        output << shot_data;

        std::system("cls");
        std::cout << output.str();

        // now do the other shit, like render the game poorly etc.
    }
    return 0;
}

// Function to handle raw input messages
LRESULT CALLBACK RawInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_INPUT) {
        RAWINPUT raw;
        UINT size = sizeof(RAWINPUT);
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

        if (raw.header.dwType == RIM_TYPEMOUSE) {
            // Process mouse input
            MouseEvent me;
            me.horizontal = raw.data.mouse.lLastX;
            me.vertical = raw.data.mouse.lLastY;
            me.timestamp = std::chrono::high_resolution_clock::now();
            me.leftMouseState = 0;

            if ((raw.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0) me.leftMouseState = 1;
            if ((raw.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) != 0) me.leftMouseState = 2;

            {
                // Lock the mutex before accessing shared data
                std::lock_guard<std::mutex> guard(mouseEventsMutex);
                // Add MouseEvent to the vector
                MouseEvents.push_back(me);
                // Close mutex by ending brackets
            }

            // Center the mouse pointer in the middle of the screen
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);
            SetCursorPos(screenWidth / 2, screenHeight / 2);
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Function to initialize raw input
void InitializeRawInput(HWND hwnd) {
    RAWINPUTDEVICE Rid[1];
    Rid[0].usUsagePage = 0x01; // Mouse
    Rid[0].usUsage = 0x02;     // Mouse
    Rid[0].dwFlags = 0;
    Rid[0].hwndTarget = hwnd;

    if (!RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]))) {
        // Handle registration failure
    }
}

HWND hwnd;

// overwatch HPMI wannabe
DWORD WINAPI DataCollector()
{

    // Create a simple window
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = RawInputProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"RawInputWindowClass";

    if (!RegisterClass(&wc)) {
        // Handle registration failure
    }

    hwnd = CreateWindowEx(
        0,
        L"RawInputWindowClass",
        L"Raw Input Window",
        WS_POPUP | WS_VISIBLE, // Full screen style
        0,
        0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (!hwnd) {
        // Handle window creation failure
    }

    // Show the window
    ShowWindow(hwnd, SW_SHOW);

    // Initialize raw input
    InitializeRawInput(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}