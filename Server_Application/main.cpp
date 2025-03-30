#include <stdio.h>
#include <string.h>
#include <enet/enet.h>

// Windows-specific headers
#ifdef _WIN32
#include <winsock2.h>
#endif

#define PORT 7777

int main() {
    ENetAddress address;
    ENetHost* server;
    ENetEvent event;

#ifdef _WIN32
    // Initialize Windows sockets
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Failed to initialize Windows sockets\n");
        return EXIT_FAILURE;
    }
#endif

    // Initialize enet
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return EXIT_FAILURE;
    }

    atexit(enet_deinitialize);

    // Configure the server address
    address.host = ENET_HOST_ANY;
    address.port = PORT;

    // Create a host (server)
    server = enet_host_create(&address, 32, 2, 0, 0);

    if (server == NULL) {
        fprintf(stderr, "An error occurred while trying to create an ENet server host.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return EXIT_FAILURE;
    }

    printf("Server started on port %d\n", PORT);

    // Main server loop
    while (true) {
        // Wait up to 1000 milliseconds for an event
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf("A new client connected from %x:%u.\n",
                    event.peer->address.host,
                    event.peer->address.port);

                // Store any relevant client information in peer->data
                //event.peer->data = "Client";
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                printf("A packet of length %zu contained %s was received from %x:%u on channel %u.\n",
                    event.packet->dataLength,
                    event.packet->data,
                    event.peer->address.host,
                    event.peer->address.port,
                    event.channelID);

                printf("Message: %s\n", event.packet->data);

                // Echo the message back to the client
                {
                    ENetPacket* packet = enet_packet_create(event.packet->data,
                        event.packet->dataLength,
                        ENET_PACKET_FLAG_RELIABLE);

                    enet_peer_send(event.peer, 0, packet);
                    enet_host_flush(server);
                }

                // Clean up the packet now that we're done using it
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("%x:%u disconnected.\n", 
                        event.peer -> address.host,
                        event.peer -> address.port);

                // Reset the peer's client information
                event.peer->data = NULL;
                break;

            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }
    }

    // Clean up
    enet_host_destroy(server);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}