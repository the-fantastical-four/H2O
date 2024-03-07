#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>  // Include for InetPton function
#include <tchar.h>
#include <chrono>  // Include the chrono library

#pragma comment(lib, "ws2_32.lib")  // Link against the Winsock library

#define NUM_PARTICLES 1000

const int PORT = 6250; 

int main() {

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock.\n";
        return 1;
    }

    // Create a socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket.\n";
        WSACleanup();
        return 1;
    }

    // Connect to the server
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    if (InetPton(AF_INET, _T("127.0.0.1"), &(serverAddr.sin_addr)) != 1) {  // Use InetPton
        std::cerr << "Invalid address.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    serverAddr.sin_port = htons(PORT); // Server port

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server.\n";

    // send requests to server 
    // this loop will be different depending on oxygen or hydrogen 
    for (int i = 0; i < NUM_PARTICLES * 2; i += 2) {

    }

    // receive responses from the server 

    char buffer[2048]; 
    int bytesReceived; 

    do {
        bytesReceived = recv(clientSocket, buffer, 2048 - 1, 0);

        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "Error in recv: " << WSAGetLastError() << std::endl;
            break;
        }
        else if (bytesReceived == 0) {
            std::cout << "Client disconnected.\n";
            break;
        }

        // Process the received data (assuming it's a null-terminated string)
        buffer[bytesReceived] = '\0';  // Ensure null-termination
        std::cout << "SERVER: " << buffer << std::endl;

    } while (bytesReceived > 0); 

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}