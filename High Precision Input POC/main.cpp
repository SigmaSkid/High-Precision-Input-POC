#include <windows.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <thread> // use windows threads? cuz I have windows defines? HAHA 

std::chrono::high_resolution_clock::time_point TheBeginningOfTime;
std::chrono::high_resolution_clock::time_point LastShotTime;

// adjust it for yourself, I use 20k dpi, this POC 
// doesn't use raw input, so take into account windows mouse speed
#define sensitivity 0.01
#define MeasureTimeDeltaMS(A) std::chrono::duration_cast<std::chrono::milliseconds>(A).count()

struct MouseEvent {
    LONG horizontal;
    LONG vertical;
    std::chrono::high_resolution_clock::time_point timestamp;
    BOOL isdownleft;

    void printData()
    {
        std::cout
            << "H: " << horizontal << std::endl
            << "V: " << vertical << std::endl
            << "L: " << isdownleft << std::endl
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
        if (me.isdownleft && 
            // shot every 100ms, cuz ak47 is 600rpm
            MeasureTimeDeltaMS(me.timestamp - LastShotTime) >= 100)
        {
            localPlayer.normalizeViewAngles();

            output << "Shot! Yaw: " << localPlayer.yaw
                << " Pitch: " << localPlayer.pitch
            << std::endl;

            LastShotTime = me.timestamp;
        }

        localPlayer.yaw += me.horizontal * sensitivity;
        localPlayer.pitch += me.vertical * sensitivity;
    }
    // # anti untrusted
    localPlayer.normalizeViewAngles();
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

        // 64 ticks, cuz cs2 does the same.
        if (ms < 15625)
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

// overwatch HPMI wannabe
DWORD WINAPI DataCollector()
{

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    std::chrono::high_resolution_clock::time_point last = std::chrono::high_resolution_clock::now();
    MouseEvent last_me;
    for (;;)
    {
        // Get current time
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        auto delta = now - last;
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(delta).count();

        // allow for 1000hz polling rate
        // if you need more than 1000 mouse inputs / second,
        // then you should go get some help.
        if (ms < 1000)
            continue;

        // just a panic key in case something goes wrong
        if (GetAsyncKeyState(VK_ESCAPE))
        {
            return -1;
        }

        last = now;

        // Create MouseEvent struct
        MouseEvent me;

        // Get current cursor position
        POINT point;
        if (GetCursorPos(&point)) {
            me.horizontal = (screenWidth / 2) - point.x;
            me.vertical = (screenHeight / 2) - point.y;
        }

        me.timestamp = now;

        // Check state of mouse buttons
        me.isdownleft = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        {
            // Lock the mutex before accessing shared data
            std::lock_guard<std::mutex> guard(mouseEventsMutex);
            // Add MouseEvent to the vector
            MouseEvents.push_back(me);
            // close mutex by ending brackets.
        }

        // me.printData();

        last_me = me;

        SetCursorPos(screenWidth / 2, screenHeight / 2);
    }
    return 0;
}