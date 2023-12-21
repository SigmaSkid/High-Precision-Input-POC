#include <windows.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <thread> // use windows threads? cuz I have windows defines? HAHA 

// this proof of concept assumes an automatic weapon.

// adjust it for yourself, I use 1k dpi, 
// keep in mind that this POC utilizes raw input!
#define sensitivity 0.002
// low tickrate makes it easy to read debug logs in real time.
// also it demonstrates pretty well, why this input method is based, as inputs
// even with the tickrate of 1 or 0.1, will be as precise as they can be.
// I recommend values of 64 or lower.
#define simulated_tickrate 1
// ak47 - 600
#define RPM 600
#define MeasureTimeDeltaMS(A) std::chrono::duration_cast<std::chrono::milliseconds>(A).count()


// converting RPM to something useful.
constexpr std::chrono::milliseconds rpmToMilliseconds(int rpm) {
    return std::chrono::milliseconds{ static_cast<long long>((60 * 1000) / rpm) };
}

// convert simulated tickrate to time between ticks accurate to the microsecond
constexpr long long tickToDelay(long double tickrate) {
    return { static_cast<long long>( ( 1./tickrate ) * 1000. * 1000.) };
}

std::chrono::high_resolution_clock::time_point TheBeginningOfTime;
std::chrono::high_resolution_clock::time_point m_flNextPrimaryAttack;

constexpr auto shotDelay = rpmToMilliseconds(RPM);
constexpr long long tickDelta = tickToDelay(simulated_tickrate);

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

    // could also be done for scoping to get the accurate accuracy penalty
    // but i'm too lazy to add that in this POC
    BOOL leftMouseDown = false;

    // when was the player last locally simulated?
    std::chrono::high_resolution_clock::time_point timestamp;

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
std::mutex consoleOutputMutex;
BasedPlayer localPlayer;
BOOL UserForceQuitCollector = FALSE;
HWND hwnd;

int main()
{
    TheBeginningOfTime = std::chrono::high_resolution_clock::now();
    m_flNextPrimaryAttack = TheBeginningOfTime;

    std::cout << "RPM:   " << RPM << std::endl
              << "Delay between shots: " << shotDelay << std::endl // chrono puts 'ms' for us <3
              << "Tickrate:   " << simulated_tickrate << " hz" << std::endl
              << "Delay between tick: " << tickDelta << " microseconds" << std::endl << std::endl;
    
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

bool canShoot(const BasedPlayer& jeff69)
{
    auto time_delta = MeasureTimeDeltaMS(jeff69.timestamp - m_flNextPrimaryAttack);
    
    // not enough time passed since last shot
    if (time_delta < 0)
        return false;

    return true;
}

ImportantEvent shoot(BasedPlayer& jeff69)
{
    jeff69.normalizeViewAngles();

    ImportantEvent event_;
    event_.pitch = jeff69.pitch;
    event_.yaw = jeff69.yaw;
    event_.timestamp = jeff69.timestamp;

    return event_;
}

// in the case of BEAM weapons, (they don't exist in counter strike, but in halo or something else maybe)
// this would just have to check when we started and when we ended holding mouse1
std::vector<ImportantEvent> DataMuncher(std::vector<MouseEvent>& mouseEvents)
{
    std::vector<ImportantEvent> ImportantEvents;
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    
    // this is really just used for keeping track of yaw & pitch, could also be used for something like scope sensitivity tho.
    // cuz I'm lazy, we will just copy the entire localPlayer for each event, this could be expensive in the context 
    // of a fully featured game.. but for me it's basically free, so I don't need to apply the aforementioned optimizations.
    std::vector<BasedPlayer> playerStateRecord;

    // let's start by adding the state from the previous tick to our nice vector.
    playerStateRecord.push_back(localPlayer);

    // create a timeline of events.
    // so we can do the shooty shoot accurately, with as real time precision as really possible.
    // the important thing we log here is the way leftMouseDown local player variable should change over time
    for (const auto& me : mouseEvents)
    {
        // look at the comments above, we really only care about some stuff from this struct here.
        BasedPlayer simulatedPlayer = playerStateRecord.back();

        // if mouse state = didn't change (0) then don't bother doing anything.
        // if mouse state = mouse key went down
        if (me.leftMouseState == 1)
        {
            simulatedPlayer.leftMouseDown = TRUE;
        }

        // if mouse state = mouse key went up
        else if (me.leftMouseState == 2)
        {
            simulatedPlayer.leftMouseDown = FALSE;
        }

        // this math could be skipped, by just saving the mouse state and timestamp
        // and then doing this later on, only for the shots, but it doesn't really matter
        // plus this is easier for me to do in a POC
        simulatedPlayer.yaw += me.horizontal * sensitivity;
        simulatedPlayer.pitch += me.vertical * sensitivity;
        simulatedPlayer.timestamp = me.timestamp;

        // store our bad boi sub-subtick player for future sub-subticks reference
        playerStateRecord.push_back(simulatedPlayer);
    }

    // this is to account for us receiving 0 mouse inputs 
    // which can happen if you're holding mouse1 while having your mouse in air, or if you're not overdosing caffeine....
    BasedPlayer eternalFool = playerStateRecord.back();
    eternalFool.timestamp = now;
    playerStateRecord.push_back(eternalFool);

    // now let's delete the player state from the last frame.
    // (we needed this fool, in the last loop)
    playerStateRecord.erase(playerStateRecord.begin());

    // used in the loop for checking if we shot between events
    auto& lastPlayerState = localPlayer;

    // now let's actually check when the shots happened. shall we?
    for (auto& playerState : playerStateRecord)
    {
        // we're trying to shoot, nice.
        if (playerState.leftMouseDown)
        
        // we can shoot, nice.
        if (canShoot(playerState))
        {
            // did we not try to shoot on the last update?
            if (!lastPlayerState.leftMouseDown)
            {
                // if we didn't then just shoot now.
                // we already checked that we can.
                auto shot = shoot(playerState);
                ImportantEvents.push_back(shot);

                // we just started shooting so adjust next shot delay accordingly.
                m_flNextPrimaryAttack = shot.timestamp + shotDelay;
            }
            // okay, we tried to shoot already,
            // now we're just holding mouse1. 
            // let's see, if the next shot should have already happened.
            else
            {
                auto time_delta = MeasureTimeDeltaMS(playerState.timestamp - m_flNextPrimaryAttack);

                // our shot should actually happen on this exact player state, down to the milisecond?
                // good enough for me.
                if (time_delta == 0)
                {
                    auto shot = shoot(playerState);
                    ImportantEvents.push_back(shot);

                    m_flNextPrimaryAttack += shotDelay;
                }
                // a shot should have happened between this update and the previous one
                else if (time_delta > 0)
                {
                    // so.. let's check when it happened, with the precision down to the milisecond
                    auto PreciseShotTiming = playerState.timestamp - std::chrono::milliseconds(time_delta);

                    // and just assume the previous mouse position is where we shot,
                    // don't interpolate this, because if the mouse moved, then was pressed, then moved
                    // we don't care about the state from after it was pressed.
                    BasedPlayer hypotheticalPlayer = lastPlayerState;
                    hypotheticalPlayer.timestamp = PreciseShotTiming;
                    auto shot = shoot(hypotheticalPlayer);
                    ImportantEvents.push_back(shot);

                    m_flNextPrimaryAttack += shotDelay;
                }
                // else if (time delta < 0)
                // will never happen because of the canShoot check,
                // in the present, we don't care about us being able to shoot in the future
            }
        }

        lastPlayerState = playerState;
    }

    // now give the results of our hard work to the local player.
    localPlayer = lastPlayerState;
    
    // # anti untrusted
    localPlayer.normalizeViewAngles();

    return ImportantEvents;
}

// use this if you don't care about performance and want the ticks to happen precisely when they should.
bool PreciseButExpensiveTimer(std::chrono::high_resolution_clock::time_point& timeTarget)
{
    return timeTarget > std::chrono::high_resolution_clock::now();
}

// our precise timer used to wait for ticks :)
bool HighQualityTimingUtilityTrustMeBro(std::chrono::high_resolution_clock::time_point& timeTarget)
{
    // get current time
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    
    // get time between the target and when we're now.
    std::chrono::nanoseconds delta = timeTarget - now;

    // convert from nano seconds to long representing amount of microseconds.
    long long ms = std::chrono::duration_cast<std::chrono::microseconds>(delta).count();

    // the next frame/tick should happen NOW
    if (ms <= 0) {
        // set the next tick target, to last target + delta
        timeTarget = timeTarget + std::chrono::microseconds(tickDelta);
        return false;
    }

    // if we need to wait less than 500 micro seconds. spin lock
    if (ms < 500)
        return true;


    // we have to wait... for more than a second? well, nice tickrate of below 1.
    if (ms > 1000000)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(ms - 2000));

        return true;
    }

    // use low latency sleep instead of spin locking.
    // it's not very precise, but it's much cheaper than spin lock
    std::this_thread::sleep_for(std::chrono::nanoseconds(250));

    // repeat.
    return true;
}


// cs2 subtick wannabe
DWORD WINAPI TickHandler()
{
    std::vector<MouseEvent> MouseEventsCopy;
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point tickTargetTime = now + std::chrono::microseconds(tickDelta);

    // wait for the raw input window to be created.
    while (!IsWindow(hwnd)) Sleep(1);

    for (;;)
    {
        if (HighQualityTimingUtilityTrustMeBro(tickTargetTime))
            continue;

        // create console output ss
        std::stringstream output;

        // store last tick time.
        auto last = now;

        // Get current time
        now = std::chrono::high_resolution_clock::now();

        // used for debugging tick timing precision
        std::chrono::nanoseconds delta = now - last;
        output << "time since last tick: " << delta.count() / 1000. / 1000. << " milliseconds." << std::endl;

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

        // output << "Just a reminder that this proof of concept creates a transparent fullscreen window, use alt+f4 if you need to exit" << std::endl;
        // who needs these disclaimers being printed anyway, am i right?

        // don't report our inputs if we didn't do anything.

        output << std::endl << "----TICK START----" << std::endl;

        if (MouseEventsCopy.size() > 0)
        output << "Received " << MouseEventsCopy.size() << " mouse events since last tick." << std::endl;

        SimplifiedDataPacket HilariousPacket;

        // also updates local player.
        HilariousPacket.shots = DataMuncher(MouseEventsCopy);

        auto timeToNextShot = MeasureTimeDeltaMS(m_flNextPrimaryAttack - now);
        auto WeCanShoot = canShoot(localPlayer);

        output << "Current YAW: " << localPlayer.yaw << "   PITCH: " << localPlayer.pitch
            << "  CURTIME: " << MeasureTimeDeltaMS(now - TheBeginningOfTime) << std::endl;
        
        if (WeCanShoot)
            output << " CAN SHOOT" << std::endl;
        else
            output << " Next shot in " << timeToNextShot << " ms" << std::endl;

        for (auto& shot : HilariousPacket.shots)
        {
            output << "logged shot: Y: " << shot.yaw << " P: " << shot.pitch << " T: " << MeasureTimeDeltaMS(shot.timestamp - TheBeginningOfTime) << std::endl;
        }
        output << "----TICK END----" << std::endl << std::endl;



        // *send* the data over our imaginary network *hotline*
        {
            std::lock_guard<std::mutex> guard(realisticNetworkMutex);
            OurImaginaryBasedNetworkingStack.push_back(HilariousPacket);
        }

        // imagine you're playing the game, while seeing numbers that represent it.
        // imagine that my proof of concept project looks more photorealistic than any game you have ever seen.
        {
            std::lock_guard<std::mutex> guard(consoleOutputMutex);
            std::cout << output.str();
        }

        // now do the other shit, like render the game poorly etc.
        // if we started shooting between frames/ticks you can interpolate the shooting animation
        // (tip: we probably, most likely, I'm 100% sure, that we did.)
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

    std::chrono::high_resolution_clock::time_point tickTargetTime = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(tickDelta);

    for (;;)
    {        
        if (HighQualityTimingUtilityTrustMeBro(tickTargetTime))
            continue;

        // create console output ss
        std::stringstream output;

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

            output << "[][][] SERVER OUT [][][]" << std::endl;
            for (const auto& shot : bigData.shots)
            {
                // do the shooty shoot and interpolate the players using the shots timestamp,
                // in the case of a real server using a chrono timestamp is dumb af, so just do a double with the
                // amount of interpolation needed between player positions in tick A and tick B
                // (THIS is tick B, tick A is the previous tick)
                // something along the lines of InterpolatePos(PosA, PosB, Fraction)
                // obviously remember to also interpolate the shooting position, even tho, it probably really doesn't matter
                // cuz if you're moving you're prolly gonna miss anyway in the case of cs2.
                output << "registered shot"
                          << "  Yaw: "    << shot.yaw 
                          << "  Pitch: "  << shot.pitch 
                          << "  Time: "   << MeasureTimeDeltaMS(shot.timestamp - TheBeginningOfTime) 
                          << std::endl;

                // if you're paranoid and want to feed your AI anticheat more data,
                // you could just send all of the mouse inputs, but imho it's a waste of cpu time, storage, bandwidth etc.
                // so we're just sending the important ones, the server should be able to use the data we gave it,
                // to check whether we could actually have shot during these timestamps, to prevent abuse of this system.
                // I'm sure the AI would love all of that data tho...
            }
            output << "[][][] SERVER END [][][]" << std::endl << std::endl;
        }

        // imagine this is the feedback of the game...
        {
            std::lock_guard<std::mutex> guard(consoleOutputMutex);
            std::cout << output.str();
        }

        if (UserForceQuitCollector)
            break;
    }

    return 0;
}