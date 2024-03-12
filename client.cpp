#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>  // Include for InetPton function
#include <tchar.h>
#include <chrono>  // Include the chrono library
#include <iomanip>

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
    //"25.14.62.92"
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

    // TODO: prompt user for a start point and end point 
    std::string moleculeType;
    int requests;
    int i = 1;
    //int step = 1;
    
    std::cout << "H or O?: ";
    std::cin >> moleculeType;

    if (moleculeType == "H") {
        i = 2;
    }

    std::cout << "Input number of requests: ";
    std::cin >> requests;

    int nRequests = requests;

    // send requests to server 
    // this loop will be different depending on oxygen or hydrogen 
    for (i; i <= nRequests * 2; i += 2) {
        int toSend = htonl(i);
        send(clientSocket, reinterpret_cast<char*>(&toSend), sizeof(toSend), 0);

        int id = (moleculeType == "H") ? i / 2 : i / 2 + 1;

        // Get the current time
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        // Convert time_t to tm as local time
        std::tm bt = *std::localtime(&timestamp);

        std::cout << moleculeType << id << ", request, " << std::put_time(&bt, "%Y-%m-%d %H:%M:%S") << std::endl;
    }
    

    // receive responses from the server 

    char buffer[2048];
    int bytesReceived;

    do {
        // Clear the buffer before each recv call
        memset(buffer, 0, sizeof(buffer));

        bytesReceived = recv(clientSocket, buffer, 2048 - 1, 0);

        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "Error in recv: " << WSAGetLastError() << std::endl;
            break;
        }
        else if (bytesReceived == 0) {
            std::cout << "Client disconnected.\n";
            break;
        }

        // Print the received data
        buffer[bytesReceived] = '\0';  // Ensure null-termination
        std::cout << buffer;

    } while (bytesReceived > 0);

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}