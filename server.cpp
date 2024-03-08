#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>

#pragma comment(lib, "ws2_32.lib") // Link with Ws2_32.lib

int PORT = 6250;

std::vector<SOCKET> clients; 
std::mutex clientMutex; 

std::queue<int> hydrogenQueue; 
std::queue<int> oxygenQueue; 

struct BondMonitor {
    std::queue<int> hydrogenQueue;
    std::queue<int> oxygenQueue;

    std::condition_variable hydrogenCond; 
    std::condition_variable oxygenCond; 

    std::unique_lock<std::mutex> condLock; // this lock is needed for C++ condition variables 

    BondMonitor() : condLock(std::mutex()) {} // initialize the monitor with a lock 

    void addToHydrogenQueue(int id) {
        hydrogenQueue.push(id); 

        if (hydrogenQueue.size() >= 2) {
            hydrogenCond.notify_one(); 
        }
    }

    void addToOxygenQueue(int id) {
        oxygenQueue.push(id); 

        oxygenCond.notify_one(); 
    }

    void bond() {
        if (oxygenQueue.empty()) {
            oxygenCond.wait(condLock, [this] { return !oxygenQueue.empty(); });
        }

        if (hydrogenQueue.size() < 2) {
            hydrogenCond.wait(condLock, [this] { return oxygenQueue.size() >= 2; });
        }

        std::cout << "O" << oxygenQueue.front() << ", bond, <timestamp>" << std::endl; 
        oxygenQueue.pop(); 
        
        std::cout << "H" << hydrogenQueue.front() << ", bond, <timestamp>" << std::endl; 
        hydrogenQueue.pop(); 

        std::cout << "H" << hydrogenQueue.front() << ", bond, <timestamp>" << std::endl;
        hydrogenQueue.pop(); 
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
                std::cout << "H" << id << ", request, <timestamp>" << std::endl; 
                // hydrogenQueue.push(id); 
                monitor.addToHydrogenQueue(id); 

            }
            else {
                int id = particleId / 2 + 1; 
                std::cout << "O" << id << ", request, <timestamp>" << std::endl;
                // oxygenQueue.push(id); 
                monitor.addToOxygenQueue(id); 
            }
        }
        
        
    }
}

void makeBonds() {
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

    acceptClientThread.join(); // wait for thread to finish

    bondThread.join(); 

    closesocket(serverSocket);
    WSACleanup();
}