#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>

#pragma comment(lib, "ws2_32.lib") // Link with Ws2_32.lib

#define HYDROGEN_CLIENT 0
#define OXYGEN_CLIENT 1 

int PORT = 6250;

std::vector<SOCKET> clients; 
std::mutex clientMutex; 

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

struct BondMonitor {
    std::queue<int> hydrogenQueue;
    std::queue<int> oxygenQueue;

    std::mutex logMutex;

    void addToHydrogenQueue(int id) {
        hydrogenQueue.push(id); 
    }

    void addToOxygenQueue(int id) {
        oxygenQueue.push(id); 
    }

    // this is just to ensure that logs aren't logging at the same time 
    void log(std::string message) {
        // Lock the mutex to ensure exclusive access to std::cout
        std::lock_guard<std::mutex> guard(logMutex);

        std::cout << message;
    }

    void sendMessageToClient(int clientIndex, std::string message) {
        int bytesSent = send(clients[clientIndex], message.c_str(), static_cast<int>(message.length()), 0);

        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "Failed to send message.\n";
        }
    }

    void bond() {

        if (!oxygenQueue.empty() && hydrogenQueue.size() >= 2) {

            std::string message; 

            message = "O" + std::to_string(oxygenQueue.front()) + ", bond, " + getCurrentTimeString() + "\n";
            log(message);
            sendMessageToClient(OXYGEN_CLIENT, message); 
            oxygenQueue.pop();

            message = "H" + std::to_string(hydrogenQueue.front()) + ", bond, " + getCurrentTimeString() + "\n";
            log(message);
            sendMessageToClient(HYDROGEN_CLIENT, message); 
            hydrogenQueue.pop();

            message = "H" + std::to_string(hydrogenQueue.front()) + ", bond, " + getCurrentTimeString() + "\n";
            log(message);
            sendMessageToClient(HYDROGEN_CLIENT, message); 
            hydrogenQueue.pop();
        }
    }
};

BondMonitor monitor; 

void handleClient(SOCKET clientSocket) {
    while (true) {
        // handle receiving requests from the clients here

        int particleId; 
        
        // receive message from client and store in particleId 
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&particleId), sizeof(particleId), 0);

        if (bytesReceived > 0) {
            particleId = ntohl(particleId);
             
            // sort received id's to their respective queues and log

            if (particleId % 2 == 0) {
                int id = particleId / 2; 
                monitor.log("H" + std::to_string(id) + ", request, " + getCurrentTimeString() + "\n");
                monitor.addToHydrogenQueue(id); 

            }
            else {
                int id = particleId / 2 + 1; 
                monitor.log("O" + std::to_string(id) + ", request, " + getCurrentTimeString() + "\n");
                monitor.addToOxygenQueue(id); 
            }
        }
        
        
    }
}

void makeBonds() {
    std::cout << "creating bonds..." << std::endl; 
    while (true) {
        monitor.bond(); 
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

    std::thread bondThread = std::thread(makeBonds);

    // Accept a client socket
    // launching a thread to accept multiple clients 
    std::thread acceptClientThread([&]() {
        
        while (clients.size() < 2) { // might change this while loop to stop after 2 iterations for 2 clients
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

    acceptClientThread.detach(); 

    bondThread.join(); 

    closesocket(serverSocket);
    WSACleanup();
}