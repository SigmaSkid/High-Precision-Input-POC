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
};

struct ImportantEvent {
    // we only care about shots being ultra accurate so
    double yaw = 0.;
    double pitch = 0.;
    std::chrono::high_resolution_clock::time_point timestamp;
};

// i'm not doing proper networking for this stuff
struct SimplifiedDataPacket {
    std::vector<ImportantEvent> shots;
    // and all the other useless data cs2 sends to the server each tick
};

struct BasedPlayer {
    double yaw = 0.;
    double pitch = 0.;
    BOOL leftMouseDown = false;
    // could also be done for scoping to get the accurate accuracy penalty
    // but i'm too lazy to add that in this POC

    void normalizeViewAngles()
    {
        // is this a good way of doing it? no. Is it a funny meme? yes.
        while (this->yaw > 180) this->yaw -= 360;
        while (this->yaw < -180) this->yaw += 360;

        this->pitch = std::clamp(pitch, -89., 89.);
    }
};

// is there any reason for this being a DWORD WINAPI? no.
DWORD WINAPI TickHandler();
DWORD WINAPI ServerWannabe();
DWORD WINAPI DataCollector();
std::vector<MouseEvent> MouseEvents;
std::mutex mouseEventsMutex;
std::vector<SimplifiedDataPacket> OurImaginaryBasedNetworkingStack;
std::mutex realisticNetworkMutex;
BasedPlayer localPlayer;
BOOL UserForceQuitCollector = FALSE;
HWND hwnd;

int main()
{
    TheBeginningOfTime = std::chrono::high_resolution_clock::now();
    LastShotTime = TheBeginningOfTime;

    std::thread a1(TickHandler); 
    std::thread a2(DataCollector);
    std::thread a3(ServerWannabe);

    // will never happen :) 
    // you can put an infinite loop here instead
    a1.join(); a2.join(); a3.join();

    for (;;)
    system("pause");

    return -1;
}

std::vector<ImportantEvent> DataMuncher(std::vector<MouseEvent>& mouseEvents)
{
    std::vector<ImportantEvent> ImportantEvents;

    for (const auto& me : mouseEvents)
    {
        if (me.leftMouseState == 1)
        {
            localPlayer.leftMouseDown = TRUE;
        }
        if (me.leftMouseState == 2)
        {
            localPlayer.leftMouseDown = FALSE;
        }

        auto delta = MeasureTimeDeltaMS(me.timestamp - LastShotTime);

        if (localPlayer.leftMouseDown &&
            // shot every 100ms, cuz ak47 is 600rpm
            delta >= 100)
        {
            localPlayer.normalizeViewAngles();

            // we shoot at this timestamp, (that's between frames), and start rendering our shot on this frame already
            ImportantEvent event_;
            event_.pitch = localPlayer.pitch;
            event_.yaw = localPlayer.yaw;
            event_.timestamp = me.timestamp;

            ImportantEvents.push_back(event_);

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
        ImportantEvent event_;
        event_.pitch = localPlayer.pitch;
        event_.yaw = localPlayer.yaw;
        event_.timestamp = now;

        ImportantEvents.push_back(event_);
        LastShotTime = now;
    }

    return ImportantEvents;
}

// cs2 subtick wannabe
DWORD WINAPI TickHandler()
{
    std::vector<MouseEvent> MouseEventsCopy;
    std::chrono::high_resolution_clock::time_point last = std::chrono::high_resolution_clock::now();

    // wait for the raw input window to be created.
    while (!IsWindow(hwnd)) Sleep(1);

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

        UserForceQuitCollector = !IsWindow(hwnd);

        if (UserForceQuitCollector)
            break;

        {
            MouseEventsCopy.clear();
            // Lock the mutex before accessing shared data
            std::lock_guard<std::mutex> guard(mouseEventsMutex);
            MouseEventsCopy = MouseEvents;
            MouseEvents.clear();
        }

        std::stringstream output;

        // output << "Just a reminder that this proof of concept creates a transparent fullscreen window, use alt+f4 if you need to exit" << std::endl;
        // who needs these disclaimers being printed anyway, am i right?

        output << "Received " << MouseEventsCopy.size() << " mouse events since last tick." << std::endl;

        // also updates yaw and pitch, so print these first
        SimplifiedDataPacket HilariousPacket;

        HilariousPacket.shots = DataMuncher(MouseEventsCopy);

        output << "Current YAW: " << localPlayer.yaw << "   PITCH: " << localPlayer.pitch << std::endl << std::endl << std::endl;


        // imagine you're playing the game, while seeing numbers that represent it.
        // imagine that my proof of concept project looks more photorealistic than any game you have ever seen.
        std::cout << output.str();
        
        // *send* the data over our imaginary network *hotline*
        {
            std::lock_guard<std::mutex> guard(realisticNetworkMutex);
            OurImaginaryBasedNetworkingStack.push_back(HilariousPacket);
        }

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

// highly realistic networking stack
DWORD WINAPI ServerWannabe() {

    std::chrono::high_resolution_clock::time_point last = std::chrono::high_resolution_clock::now();

    for (;;)
    {        
        // Get current time
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        auto delta = now - last;
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(delta).count();

        // wait for next tick/frame (which isn't real in this POC)
        if (ms < (1. / simulated_tickrate) * 1000 * 1000)
            continue;

        last = now;
        INT size = 0;

        {
            std::lock_guard<std::mutex> guard(realisticNetworkMutex);
            size = OurImaginaryBasedNetworkingStack.size();
            if (size == 0)
                continue;
        }
        
        for (int i = 0; i < size; i++) {

            SimplifiedDataPacket bigData;

            {
                std::lock_guard<std::mutex> guard(realisticNetworkMutex);
                if (OurImaginaryBasedNetworkingStack.size() == 0)
                    break;

                bigData = OurImaginaryBasedNetworkingStack.front();
                OurImaginaryBasedNetworkingStack.erase(OurImaginaryBasedNetworkingStack.begin());
            }

            for (const auto& shot : bigData.shots)
            {
                // do the shooty shoot and interpolate the players using the shots timestamp,
                // in the case of a real server using a chrono timestamp is dumb af, so just do a double with the
                // amount of interpolation needed between player positions in tick A and tick B
                // something along the lines of InterpolatePos(PosA, PosB, Fraction)
                std::cout << "Server registered the shot!" << std::endl;

            }
        }
        if (UserForceQuitCollector)
            break;
    }

    return 0;
}