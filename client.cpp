#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>  // Include for InetPton function
#include <tchar.h>
#include <chrono>  // Include the chrono library
#include <iomanip>
#include <sstream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")  // Link against the Winsock library

#define NUM_PARTICLES 1000

const int PORT = 6250;

auto getCurrentTime() {
    return std::chrono::system_clock::now(); 
}

std::string getCurrentTimeString() {

    // Get the current time
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);

    // Convert time_t to tm as local time
    std::tm bt = *std::localtime(&timestamp);

    // Format the date and time
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");

    // Return the formatted time as a string
    return oss.str();

}

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

    // Set socket to non-blocking mode
    u_long mode = 1; // 1 for non-blocking, 0 for blocking
    if (ioctlsocket(clientSocket, FIONBIO, &mode) != 0) {
        std::cerr << "Error setting non-blocking mode: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Connect to the server
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    //"25.14.62.92"
    if (InetPton(AF_INET, _T("25.17.98.165"), &(serverAddr.sin_addr)) != 1) {  // Use InetPton
        std::cerr << "Invalid address.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    serverAddr.sin_port = htons(PORT); // Server port

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
            std::cerr << "Failed to connect to server.\n";
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }
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
    auto earliestTime = getCurrentTime(); 
    for (i; i <= nRequests * 2; i += 2) {
        int toSend = htonl(i);
        send(clientSocket, reinterpret_cast<char*>(&toSend), sizeof(toSend), 0);

        int id = (moleculeType == "H") ? i / 2 : i / 2 + 1;

        std::cout << moleculeType << id << ", request, " << getCurrentTimeString() << std::endl;
    }
    

    // receive responses from the server 

    char buffer[32768];
    int bytesReceived;

    int responseReceived = 0; 

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(clientSocket, &readSet);

    std::chrono::system_clock::time_point latestTime; 

    while(responseReceived < nRequests) {
        int selectResult = select(0, &readSet, NULL, NULL, NULL);

        if (selectResult == SOCKET_ERROR) {
            std::cerr << "Error in select: " << WSAGetLastError() << std::endl;
            break;
        }

        // Data is available to be received
        // Clear the buffer before each recv call
        memset(buffer, 0, sizeof(buffer));

        bytesReceived = recv(clientSocket, buffer, 32768, 0);

        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                std::cerr << "Error in recv: " << error << std::endl;
                break;
            }
        }
        else if (bytesReceived > 0) {
            // Print the received data
            // buffer[bytesReceived] = '\0';  // Ensure null-termination
            // std::cout << buffer << getCurrentTimeString() << std::endl;

            std::istringstream iss(buffer);
            std::string message;
            while (std::getline(iss, message, '/')) {
                // Print or process each message individually
                std::cout << message << getCurrentTimeString() << std::endl;
                responseReceived++; 
                if (responseReceived == nRequests) {
                    latestTime = getCurrentTime(); 
                }
            }
        }

    }

    int end = -1;
    int endSymbol = htonl(end);
    send(clientSocket, reinterpret_cast<char*>(&endSymbol), sizeof(endSymbol), 0);

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();

    auto duration = latestTime - earliestTime; 

    double seconds = std::chrono::duration<double>(duration).count();

    std::cout << "Number of seconds elapsed: " << seconds << std::endl;

    return 0;
}