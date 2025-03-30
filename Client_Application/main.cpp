#include <stdio.h>
#include <string.h>
#include <enet/enet.h>

// Windows-specific headers
#ifdef _WIN32
#include <winsock2.h>
#endif

#define SERVER_ADDRESS "127.0.0.1"
#define PORT 7777

int main() {
    ENetHost* client;
    ENetAddress address;
    ENetPeer* peer;
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

    // Create a client host
    client = enet_host_create(NULL, 1, 2, 0, 0);

    if (client == NULL) {
        fprintf(stderr, "An error occurred while trying to create an ENet client host.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return EXIT_FAILURE;
    }

    // Set the server address to connect to
    enet_address_set_host(&address, SERVER_ADDRESS);
    address.port = PORT;

    // Connect to the server
    peer = enet_host_connect(client, &address, 2, 0);

    if (peer == NULL) {
        fprintf(stderr, "No available peers for initiating an ENet connection.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return EXIT_FAILURE;
    }

    // Wait up to 5 seconds for the connection attempt to succeed
    if (enet_host_service(client, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        printf("Connection to %s:%d succeeded.\n", SERVER_ADDRESS, PORT);
    }
    else {
        enet_peer_reset(peer);
        fprintf(stderr, "Connection to %s:%d failed.\n", SERVER_ADDRESS, PORT);
#ifdef _WIN32
        WSACleanup();
#endif
        return EXIT_FAILURE;
    }

    // Send a test message
    {
        const char* message = "Hello, ENet!";
        ENetPacket* packet = enet_packet_create(message,
            strlen(message) + 1,
            ENET_PACKET_FLAG_RELIABLE);

        enet_peer_send(peer, 0, packet);
        enet_host_flush(client);
    }

    // Main client loop
    int running = 1;
    while (running) {
        // Wait up to 1000 milliseconds for an event
        while (enet_host_service(client, &event, 1000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                printf("A packet of length %zu contained %s was received from %x:%u on channel %u.\n",
                    event.packet->dataLength,
					event.packet->data,
					event.peer->address.host,
					event.peer->address.port,
                    event.channelID);

                printf("Server response: %s\n", event.packet->data);

                // Clean up the packet now that we're done using it
                enet_packet_destroy(event.packet);

                // Send another message or exit
                printf("Type a message (or 'quit' to exit): ");
                {
                    char buffer[1024];
                    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                        // Remove trailing newline
                        buffer[strcspn(buffer, "\n")] = 0;

                        if (strcmp(buffer, "quit") == 0) {
                            running = 0;
                            break;
                        }

                        ENetPacket* packet = enet_packet_create(buffer,
                            strlen(buffer) + 1,
                            ENET_PACKET_FLAG_RELIABLE);

                        enet_peer_send(peer, 0, packet);
                        enet_host_flush(client);
                    }
                }
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("Server disconnected.\n");
                running = 0;
                break;

            default:
                break;
            }
        }
    }

    // Clean up
    enet_peer_disconnect(peer, 0);

    // Wait up to 3 seconds for the disconnect to succeed
    while (enet_host_service(client, &event, 3000) > 0) {
        if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            printf("Disconnection succeeded.\n");
            break;
        }
    }

    enet_host_destroy(client);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}