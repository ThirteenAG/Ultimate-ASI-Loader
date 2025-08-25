#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

// Command structure matching the updated client
struct ServerCommand
{
    enum Type : uint32_t
    {
        ADD_FILE = 1,
        APPEND_FILE = 2,
        REMOVE_FILE = 3,
        GET_SIZE = 4,
        READ_FILE = 5
    };
    uint32_t command;
    uint64_t server_handle; // Used for most commands
    uint64_t data_size;     // For ADD/APPEND/READ
    int32_t priority;       // For ADD
    uint64_t offset;        // For READ
};

// Represents a file stored in the server's memory
struct StoredFile
{
    std::vector<uint8_t> data;
    int32_t priority;
};

// Global map to store files, keyed by a unique handle
std::unordered_map<uint64_t, std::shared_ptr<StoredFile>> g_serverFileSystem;
std::mutex g_fileSystemMutex;
std::atomic<uint64_t> g_nextHandle = 1; // Start handles from 1

// Handles a single client connection
void HandleClient(HANDLE hPipe)
{
    DWORD clientPid = 0;
    DWORD bytesRead = 0;
    if (!ReadFile(hPipe, &clientPid, sizeof(clientPid), &bytesRead, NULL) || bytesRead != sizeof(clientPid))
    {
        std::wcerr << L"Server: Failed to read client PID." << std::endl;
        return;
    }
    std::wcout << L"Server: Connected to client with PID: " << clientPid << std::endl;

    HANDLE hClientProcess = OpenProcess(SYNCHRONIZE, FALSE, clientPid);

    while (true)
    {
        if (hClientProcess != NULL && WaitForSingleObject(hClientProcess, 0) == WAIT_OBJECT_0)
        {
            std::wcout << L"Server: Client process " << clientPid << " has terminated. Closing connection." << std::endl;
            CloseHandle(hClientProcess);
            break;
        }

        ServerCommand cmd = { 0 };
        bytesRead = 0;
        BOOL bSuccess = ReadFile(hPipe, &cmd, sizeof(ServerCommand), &bytesRead, NULL);

        if (!bSuccess || bytesRead == 0)
        {
            if (GetLastError() == ERROR_BROKEN_PIPE)
                std::wcout << L"Server: Client disconnected." << std::endl;
            else
                std::wcerr << L"Server: ReadFile failed with error " << GetLastError() << std::endl;
            break;
        }

        std::lock_guard<std::mutex> lock(g_fileSystemMutex);
        switch (cmd.command)
        {
        case ServerCommand::ADD_FILE:
        {
            std::vector<uint8_t> data(cmd.data_size);
            if (cmd.data_size > 0)
            {
                if (!ReadFile(hPipe, data.data(), (DWORD)cmd.data_size, &bytesRead, NULL) || bytesRead != cmd.data_size)
                {
                    std::wcerr << L"Server: Failed to read data for ADD_FILE." << std::endl;
                    continue;
                }
            }

            uint64_t newHandle = g_nextHandle.fetch_add(1);
            auto newFile = std::make_shared<StoredFile>();
            newFile->data = std::move(data);
            newFile->priority = cmd.priority;
            g_serverFileSystem[newHandle] = newFile;

            DWORD bytesWritten;
            if (!WriteFile(hPipe, &newHandle, sizeof(newHandle), &bytesWritten, NULL) || bytesWritten != sizeof(newHandle))
            {
                std::wcerr << L"Server: Failed to send new handle to client." << std::endl;
            }
            std::wcout << L"Server: ADD_FILE successful. Handle: " << newHandle << std::endl;
            break;
        }
        case ServerCommand::APPEND_FILE:
        {
            auto it = g_serverFileSystem.find(cmd.server_handle);
            if (it != g_serverFileSystem.end())
            {
                std::vector<uint8_t> dataToAppend(cmd.data_size);
                if (cmd.data_size > 0)
                {
                    if (!ReadFile(hPipe, dataToAppend.data(), (DWORD)cmd.data_size, &bytesRead, NULL) || bytesRead != cmd.data_size)
                    {
                        std::wcerr << L"Server: Failed to read data for APPEND_FILE." << std::endl;
                        continue;
                    }
                    it->second->data.insert(it->second->data.end(), dataToAppend.begin(), dataToAppend.end());
                    std::wcout << L"Server: APPEND_FILE successful for handle: " << cmd.server_handle << std::endl;
                }
            }
            break;
        }
        case ServerCommand::REMOVE_FILE:
        {
            g_serverFileSystem.erase(cmd.server_handle);
            std::wcout << L"Server: REMOVE_FILE for handle: " << cmd.server_handle << std::endl;
            break;
        }
        case ServerCommand::READ_FILE:
        {
            DWORD bytesToSend = 0;
            std::vector<uint8_t> buffer;
            auto it = g_serverFileSystem.find(cmd.server_handle);
            if (it != g_serverFileSystem.end())
            {
                const auto& storedFile = it->second;
                if (cmd.offset < storedFile->data.size())
                {
                    uint64_t remainingBytes = storedFile->data.size() - cmd.offset;
                    bytesToSend = static_cast<DWORD>(min((uint64_t)cmd.data_size, remainingBytes));
                    if (bytesToSend > 0)
                    {
                        buffer.resize(bytesToSend);
                        memcpy(buffer.data(), storedFile->data.data() + cmd.offset, bytesToSend);
                    }
                }
            }

            DWORD bytesWritten;
            if (!WriteFile(hPipe, &bytesToSend, sizeof(bytesToSend), &bytesWritten, NULL)) break;
            if (bytesToSend > 0)
            {
                if (!WriteFile(hPipe, buffer.data(), bytesToSend, &bytesWritten, NULL)) break;
            }
            break;
        }
        default:
            std::wcerr << L"Server: Unknown command received: " << cmd.command << std::endl;
            break;
        }
    }

    if (hClientProcess) CloseHandle(hClientProcess);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}

int main()
{
    const wchar_t* pipeName = L"\\\\.\\pipe\\Ultimate-ASI-Loader-VirtualFileServer";
    std::wcout << L"Server: Starting virtual file server..." << std::endl;

    while (true)
    {
        HANDLE hPipe = CreateNamedPipeW(
            pipeName, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            std::wcerr << L"Server: CreateNamedPipe failed, GLE=" << GetLastError() << std::endl;
            return 1;
        }

        std::wcout << L"Server: Pipe created. Waiting for client connection..." << std::endl;

        if (ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED))
        {
            std::thread(HandleClient, hPipe).detach();
        }
        else
        {
            CloseHandle(hPipe);
        }
    }

    return 0;
}