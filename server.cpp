#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>

#pragma comment(lib, "ws2_32.lib") // Link with Ws2_32.lib

int PORT = 6250;

std::vector<SOCKET> clients; 
std::mutex clientMutex; 

std::queue<int> hydrogenQueue; 
std::queue<int> oxygenQueue; 

void handleClient(SOCKET clientSocket) {
    while (true) {
        // handle receiving requests from the clients here
    }
}

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock.\n";
        return 1;
    }

    // Create a socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket.\n";
        WSACleanup();
        return 1;
    }

    // Bind the socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT); // You can choose any port

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << PORT << "...\n";

    // Accept a client socket
    // launching a thread to accept multiple clients 
    std::thread acceptClientThread([&]() {
        
        while (true) { // might change this while loop to stop after 2 iterations for 2 clients
            SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
                break;
            }

            std::cout << "Client connected.\n";

            {
                std::lock_guard<std::mutex> lock(clientMutex);
                clients.push_back(clientSocket); 
            }

            std::thread(handleClient, clientSocket).detach(); // main thread no longer waits for child thread to finish 
        }

        }); 

    acceptClientThread.join(); // wait for thread to finish

    // bonding logic 
    while (true) { 

    }

    closesocket(serverSocket);
    WSACleanup();
}