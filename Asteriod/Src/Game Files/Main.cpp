#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <memory>
#include <thread>

#include "main.h"
#include "NetworkManager.h"

#pragma comment(lib, "Ws2_32.lib")

// ---------------------------------------------------------------------------
// Globals
float g_dt;
double g_appTime;

int WINAPI WinMain(HINSTANCE instanceH, HINSTANCE prevInstanceH, LPSTR command_line, int show) {
    UNREFERENCED_PARAMETER(prevInstanceH);
    UNREFERENCED_PARAMETER(command_line);

    // Initialize the system
    AESysInit(instanceH, show, 800, 600, 1, 60, false, NULL);

    // Changing the window title
    AESysSetWindowTitle("Asteroids with Multiplayer!");

    // Set background color
    AEGfxSetBackgroundColor(0.0f, 0.0f, 0.0f);

    // Initialize game state manager
    GameStateMgrInit(GS_ASTEROIDS);

    // Initialize network manager
    NetworkManager networkManager;
    if (networkManager.Initialize(true)) {
        // Run the network manager in a separate thread or integrate it into the game loop
        // std::thread networkThread(&NetworkManager::Run, &networkManager);
        // networkThread.detach();
    }

    while (gGameStateCurr != GS_QUIT) {
        // Reset the system modules
        AESysReset();

        // If not restarting, load the game state
        if (gGameStateCurr != GS_RESTART) {
            GameStateMgrUpdate();
            GameStateLoad();
        }
        else {
            gGameStateNext = gGameStateCurr = gGameStatePrev;
        }

        // Initialize the game state
        GameStateInit();

        while (gGameStateCurr == gGameStateNext) {
            AESysFrameStart();

            GameStateUpdate();
            GameStateDraw();

            AESysFrameEnd();

            // Check if forcing the application to quit
            if ((AESysDoesWindowExist() == false) || AEInputCheckTriggered(AEVK_ESCAPE)) {
                gGameStateNext = GS_QUIT;
            }

            g_dt = (f32)AEFrameRateControllerGetFrameTime();
            g_appTime += g_dt;
        }

        GameStateFree();

        if (gGameStateNext != GS_RESTART) {
            GameStateUnload();
        }

        gGameStatePrev = gGameStateCurr;
        gGameStateCurr = gGameStateNext;
    }

    // Free the system
    AESysExit();
}