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
#include <set>

#pragma comment(lib, "ws2_32.lib") // Link with Ws2_32.lib

#define HYDROGEN_CLIENT 0
#define OXYGEN_CLIENT 1 

int PORT = 6250;

std::vector<SOCKET> clients; 
std::mutex clientMutex;

int hydrogenRequests = 0; 
int oxygenRequests = 0; 
int hydrogensBonded = 0;
int oxygensBonded = 0;

std::chrono::system_clock::time_point earliestTimeO;
std::chrono::system_clock::time_point earliestTimeH; 
std::chrono::system_clock::time_point latestTime; 

int errors = 0; 

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

bool isInSet(std::set<int> set, int element) {
    if (set.find(element) != set.end()) {
        return false; 
    }

    return true; 
}

struct BondMonitor {
    std::queue<int> hydrogenQueue;
    std::queue<int> oxygenQueue;

    std::set<int> hydrogenSet; 
    std::set<int> oxygenSet; 

    std::mutex logMutex;

    void addToHydrogenQueue(int id) {
        hydrogenQueue.push(id); 
        hydrogenSet.insert(id); 
    }

    void addToOxygenQueue(int id) {
        oxygenQueue.push(id); 
        oxygenSet.insert(id); 
    }

    // this is just to ensure that logs aren't logging at the same time 
    void log(std::string message) {
        // Lock the mutex to ensure exclusive access to std::cout
        std::lock_guard<std::mutex> guard(logMutex);

        std::cout << message << getCurrentTimeString() << std::endl;
    }

    void sendMessageToClient(int clientIndex, std::string message) {
        std::string toSend = message + "/";
        int bytesSent = send(clients[clientIndex], toSend.c_str(), static_cast<int>(toSend.length()), 0);

        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "Failed to send message.\n";
        }
    }

    void bond() {

        if (!oxygenQueue.empty() && hydrogenQueue.size() >= 2) {

            if (oxygenQueue.empty() || hydrogenQueue.size() < 2) {
                errors++; 
                std::cout << "Insufficient number of atoms to bond" << std::endl; 
            }

            std::string message; 

            int front = oxygenQueue.front(); 
            if (!isInSet(oxygenSet, front)) {
                errors++; 
                std::cout << "Invalid bond" << front << std::endl; 
            }
            message = "O" + std::to_string(oxygenQueue.front()) + ", bond, ";
            log(message);
            sendMessageToClient(OXYGEN_CLIENT, message); 
            oxygenQueue.pop();
            oxygensBonded++; 

            front = hydrogenQueue.front(); 
            if (!isInSet(hydrogenSet, front)) {
                errors++;
                std::cout << "Invalid bond no request H" << front << std::endl;
            }
            message = "H" + std::to_string(hydrogenQueue.front()) + ", bond, ";
            log(message);
            sendMessageToClient(HYDROGEN_CLIENT, message); 
            hydrogenQueue.pop();
            hydrogensBonded++;

            front = hydrogenQueue.front();
            if (!isInSet(hydrogenSet, front)) {
                errors++;
                std::cout << "Invalid bond no request H" << front << std::endl;
            }
            message = "H" + std::to_string(hydrogenQueue.front()) + ", bond, ";
            log(message);
            sendMessageToClient(HYDROGEN_CLIENT, message); 
            hydrogenQueue.pop();
            hydrogensBonded++; 
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
                if (hydrogenRequests == 0) {
                    earliestTimeH = std::chrono::system_clock::now();
                    hydrogenRequests++;
                }
                int id = particleId / 2; 
                monitor.log("H" + std::to_string(id) + ", request, ");
                monitor.addToHydrogenQueue(id); 
            }
            else {
                if (oxygenRequests == 0) {
                    earliestTimeO = std::chrono::system_clock::now();
                    oxygenRequests++;
                }
                int id = particleId / 2 + 1; 
                monitor.log("O" + std::to_string(id) + ", request, ");
                monitor.addToOxygenQueue(id); 
            }
        }
        
        
    }
}

void makeBonds(int expectedH, int expectedO) {
    std::cout << "creating bonds..." << std::endl; 
    while (oxygensBonded < expectedO || hydrogensBonded < expectedH) {
        monitor.bond(); 
    }
    latestTime = std::chrono::system_clock::now(); 
    std::this_thread::sleep_for(std::chrono::seconds(1));
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

    int expectedH; 
    std::cout << "Enter number of Hydrogen: ";
    std::cin >> expectedH; 

    int expectedO; 
    std::cout << "Enter number of Oxygen: "; 
    std::cin >> expectedO; 

    std::thread bondThread = std::thread(makeBonds, expectedH, expectedO);

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

            // Set socket to non-blocking mode
            u_long mode = 1; // 1 for non-blocking, 0 for blocking
            if (ioctlsocket(clientSocket, FIONBIO, &mode) != 0) {
                std::cerr << "Error setting non-blocking mode: " << WSAGetLastError() << std::endl;
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }

            {
                std::lock_guard<std::mutex> lock(clientMutex);
                clients.push_back(clientSocket); 
            }

            std::thread(handleClient, clientSocket).detach(); // main thread no longer waits for child thread to finish 
        }

        }); 

    acceptClientThread.detach(); 

    bondThread.join(); 

    auto duration = (earliestTimeH < earliestTimeO) ? latestTime - earliestTimeH : latestTime - earliestTimeO;
    double seconds = std::chrono::duration<double>(duration).count();

    std::cout << "Number of seconds elapsed: " << seconds << std::endl;
    std::cout << "Number of errors: " << errors << std::endl; 

    closesocket(serverSocket);
    WSACleanup();
}