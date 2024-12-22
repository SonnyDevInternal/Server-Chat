#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <format>
#include <iphlpapi.h>
#include <thread>
#include <functional>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "iphlpapi.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "69420"

class ServerApplication
{
private:
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

public:
    static ServerApplication* Object;
    std::string OwnIp = "";

    ServerApplication() {
        Object = this;
    }

    static void LogServer(std::string Message) {
        std::cout << "[LOG] : " << Message << "\n";
    }

    int __cdecl MainServer(void);
    void AcceptUsersLoop();
    std::string getMacAddress(std::string ipAddress);
    std::string GetIPString(sockaddr Address);
    std::string GetOwnIp();

    void SendToClientsMsgs(struct Message_ Message, bool SendKey);
    void SendMsgToClient(struct Message_ Msg__, int Index, bool SendKey);
    void BanClient(struct User* user);
};

using EventHandler = std::function<void()>;

class EventSource {
public:
    // Register a function as an event handler
    void RegisterHandler(EventHandler handler) {
        handlers.push_back(handler);
    }

    void UnregisterHandlers() {
        handlers.clear();
    }
    // Trigger the event, invoking all registered handlers
    void TriggerEvent() {
        for (const auto& handler : handlers) {
            handler();
        }
    }

private:
    std::vector<EventHandler> handlers;
};

struct User {
public:
    static std::vector<User*> UsersConnected;

    int index = 0;
    std::string Username = "";
    std::string IP;
    std::string Mac;
    SOCKET UserSocket;
    std::thread receiveThread;
    std::vector<std::string> MessageHistory;

    void MessageReceived(std::string Msg);

    User(std::string IP, std::string Mac, SOCKET CorrespondingSocketUser)
    {
        this->UserSocket = CorrespondingSocketUser;
        this->IP = IP;
        this->Mac = Mac;
        this->index = UsersConnected.size();
        this->receiveThread = std::thread(&User::ReceiveShitFromClients, this);

        this->receiveThread.detach();

        UsersConnected.push_back(this);
    }


    ~User()
    {

        for (size_t i = 0; i < UsersConnected.size(); i++)
        {
            if (UsersConnected[i] == this)
            {
                UsersConnected.erase(UsersConnected.begin() + i);
                break;
            }
        }
    }

    void DisconnectClient();

    void ReceiveShitFromClients();


    void SetUsername(std::string Username)
    {
        this->Username = Username;
    }


    struct UserCommand
    {
        UserCommand(EventHandler handler, std::string CommandName) {
            CommandEvent.RegisterHandler(handler);

            cmdName = CommandName;
        }

        bool Activate(User* UserActivating);

        std::string cmdName = "";
        std::vector<std::string> Parameters;
        EventSource CommandEvent;

        bool WasEventSuccessfull = false;
    };


    static std::vector<UserCommand> CommandsActive;

    bool ActivateCommand(std::string Input);

    UserCommand* FindCommand(std::string cmdName, std::vector<std::string> Params);

    static void InitializeCommands();

};

struct Message_ {
    struct ByteType {
        int Size;
        std::vector<BYTE> Data_;




        ByteType(std::vector<BYTE>* DataReference, ULONGLONG& DataCounter)
        {

            std::vector<BYTE> DataCopy;

            this->Size = 0;

            memcpy(&this->Size, DataReference->data() + DataCounter, sizeof(int));


            if (this->Size >= DEFAULT_BUFLEN) {
                std::exception except = std::exception("Faulty Data was passed!");
                throw except;
            }

            DataCounter += 4;

            for (int i = 0; i < this->Size; i++, DataCounter++)
            {
                DataCopy.push_back(DataReference[0][DataCounter]);
            }


            this->Data_ = DataCopy;
        }

        std::string GetString()
        {
            std::string output__;

            for (size_t i = 0; i < this->Size; i++)
            {
                output__ += Data_[i];
            }

            return output__;
        }

        bool GetBool()
        {
            if (Size < 1)throw "Type mismatch";

            bool out;

            memcpy(&out, Data_.data(), sizeof(bool));

            return out;
        }

        int GetInt()
        {
            if (Size < 4)throw "Type mismatch";

            int out;

            memcpy(&out, Data_.data(), sizeof(int));

            return out;
        }

        ULONGLONG GetULONGLONG()
        {
            if (Size < sizeof(ULONGLONG))throw "Type mismatch";

            ULONGLONG out;

            memcpy(&out, Data_.data(), sizeof(ULONGLONG));

            return out;
        }
    };


public:
    std::string Username;
    std::string Message;
    ULONGLONG EncryptionKey = 0;
    size_t PackedMsgSize = 0;

private:
    void* MsgCreated = nullptr;

public:

    void UnpackMessage(const char* MessagePacked, size_t PackedMsgSize_)
    {
        this->PackedMsgSize = PackedMsgSize_;

        Username = "";
        Message = "";

        ULONGLONG DataCounter = 0;
        std::vector<BYTE> bytearray;

        size_t len = PackedMsgSize;

        for (size_t i = 0; i < len; i++)
        {
            bytearray.push_back(MessagePacked[i]);
        }

        auto type = ByteType(&bytearray, DataCounter);

        Username = type.GetString();

        type = ByteType(&bytearray, DataCounter);

        Message = type.GetString();

        if (DataCounter < bytearray.size() && Message == "") {
            type = ByteType(&bytearray, DataCounter);

            EncryptionKey = type.GetULONGLONG();
        }

        DecryptMessage();
    }

    const char* PackMessage(bool SendKey) {
        std::string PackedData = "";

        std::vector<BYTE> Data;

        Data.reserve(4);

        {//String 1

            int Size__ = Username.size();

            auto bytesize = (char*)&Size__;

            for (size_t i = 0; i < 4; i++)
            {
                Data.push_back(bytesize[i]);
            }

            for (size_t i = 0; i < Data.size(); i++)
            {
                PackedData += (char)Data[i];
            }


            for (size_t i = 0; i < Username.length(); i++)
            {
                PackedData += Username[i];
            }

            // PackedData = Username.c_str();
        }

        {//String 2
            Data.clear();


            int Size__ = Message.size();

            auto bytesize = (char*)&Size__;

            for (size_t i = 0; i < 4; i++)
            {
                Data.push_back(bytesize[i]);
            }

            for (size_t i = 0; i < Data.size(); i++)
            {
                PackedData += Data[i];
            }

            for (size_t i = 0; i < Message.length(); i++)
            {
                PackedData += Message[i];
            }

        }

        if (SendKey) {//EncryptionKey
            Data.clear();

            int Size__ = 8;

            auto bytesize = (char*)&Size__;

            for (size_t i = 0; i < 4; i++)
            {
                Data.push_back(bytesize[i]);
            }

            for (size_t i = 0; i < Data.size(); i++)
            {
                PackedData += Data[i];
            }


            Data.clear();

            ULONGLONG KeyEnc = EncryptionKey + 19202941;

            bytesize = (char*)&KeyEnc;

            for (size_t i = 0; i < 8; i++)
            {
                Data.push_back(bytesize[i]);
            }

            for (size_t i = 0; i < Data.size(); i++)
            {
                PackedData += Data[i];
            }
        }

        if (this->MsgCreated) {
            delete[](const char*)this->MsgCreated;
            this->MsgCreated = nullptr;
        }

        const char* output = new char[PackedData.length()];

        std::memcpy((void*)output, PackedData.data(), PackedData.length());

        this->MsgCreated = (void*)output;

        PackedMsgSize = PackedData.size();

        return output;
    }

    std::string XorString(const std::string& input, const ULONGLONG& key) {
        std::string result;
        for (size_t i = 0; i < input.size(); ++i) {
            result.push_back(input[i] ^ (key >> (i % sizeof(ULONGLONG))));
        }
        return result;
    }

    void DecryptMessage()
    {
        if (EncryptionKey != 0) {
            //Add Decrpyting algorithm here!

            Message = XorString(Message, EncryptionKey);
            Username = XorString(Username, EncryptionKey);
        }

    }

    void EncryptMessage()
    {
        if (EncryptionKey != 0) {
            //Add Encrypting algorithm here!

            Message = XorString(Message, EncryptionKey);
            Username = XorString(Username, EncryptionKey);
        }

    }

    ~Message_()
    {
        if (this->MsgCreated)
            delete[](const char*)this->MsgCreated;
    }
};




