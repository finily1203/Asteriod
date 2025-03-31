#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
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

NetworkManager networkManager;

void DisplayServerInfo(const std::string& localIP, int port) {
    std::cout << "Server IP Address: " << localIP << std::endl;
    std::cout << "Server UDP Port: " << port << std::endl;
}

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

    bool isServer = true; // Set to true if this instance is the server
    int port = 8888; // Set the UDP port number

    if (networkManager.Initialize(isServer)) {
        // Display server info
        DisplayServerInfo(networkManager.GetServerIp(), port);

        // Run the network manager in a separate thread or integrate it into the game loop
        std::thread networkThread(&NetworkManager::Run, &networkManager);
        networkThread.detach();

        Player player;
        player.SetID(0);
        player.SetConnected(true);
        player.SetScore(0);

        // Add the first player and set as connected
        networkManager.GetPlayers().push_back(player);

    }
    else {
        std::cerr << "Failed to initialize network manager." << std::endl;
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

            if (isServer) {
                // Process server input
                unsigned char inputFlags = 0;
                if (AEInputCheckCurr(AEVK_UP)) inputFlags |= INPUT_UP;
                if (AEInputCheckCurr(AEVK_DOWN)) inputFlags |= INPUT_DOWN;
                if (AEInputCheckCurr(AEVK_LEFT)) inputFlags |= INPUT_LEFT;
                if (AEInputCheckCurr(AEVK_RIGHT)) inputFlags |= INPUT_RIGHT;
                if (AEInputCheckCurr(AEVK_SPACE)) inputFlags |= INPUT_FIRE;
                networkManager.ProcessPlayerInput(networkManager.GetPlayerID(), inputFlags);

                // Update game state
                networkManager.UpdateGameState();
            }
            else {
                // Send player input to server
                unsigned char inputFlags = 0;
                if (AEInputCheckCurr(AEVK_UP)) inputFlags |= INPUT_UP;
                if (AEInputCheckCurr(AEVK_DOWN)) inputFlags |= INPUT_DOWN;
                if (AEInputCheckCurr(AEVK_LEFT)) inputFlags |= INPUT_LEFT;
                if (AEInputCheckCurr(AEVK_RIGHT)) inputFlags |= INPUT_RIGHT;
                if (AEInputCheckCurr(AEVK_SPACE)) inputFlags |= INPUT_FIRE;
                networkManager.SendPlayerInput(networkManager.GetPlayerID(), inputFlags);

                // Receive game state updates from server
                networkManager.ReceiveGameState();

                // Handle acknowledgments
                networkManager.HandleAcknowledgments();
            }
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

    // Cleanup Winsock
    WSACleanup();

    return 0;
}