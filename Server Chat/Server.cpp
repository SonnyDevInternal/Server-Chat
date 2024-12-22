#include "Server.h"

namespace Commands {
    User* UserActivated = nullptr;
    User::UserCommand* CommandExecuting = nullptr;

    void KysCommand();
    void GetUserCountCommand();
    void BanUserCommand();
}


std::string ToLower(std::string Input) {

    std::string Output = "";

    for (size_t i = 0; i < Input.size(); i++)
    {
        Output += std::tolower(Input[i]);
    }

    return Output;
}



int __cdecl ServerApplication::MainServer(void)
{
    WSADATA wsaData;
    int iResult;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    this->OwnIp = GetOwnIp();

    std::cout << "Server IP:" << this->OwnIp << "\n";

    std::thread MainThread(&ServerApplication::AcceptUsersLoop, this);

    MainThread.join();


    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}


void User::MessageReceived(std::string Msg) {
    if (MessageHistory.size() >= 120) {

        std::vector<std::string> MessageCopy;

        for (size_t i = 50; i < 120; i++)
        {
            MessageCopy.push_back(MessageHistory[i]);
        }

        MessageHistory = MessageCopy;
    }

    MessageHistory.push_back(Msg);

}

bool User::ActivateCommand(std::string Input) {
    if (Input.size() >= 2 && Input[0] == '/') {

        int State = 0;

        std::string cmdName = "";
        std::vector<std::string> Parameters;



        for (size_t i = 1; i < Input.size(); i++)
        {
            if (Input[i] == ' ') { 
                State++;
                if (State >= 1)Parameters.push_back("");
                continue;
            }

            switch (State)
            {
            case 0:
                cmdName += Input[i];
                break;

            default:
                if(Parameters.size() != 0)
                Parameters[Parameters.size() - 1] += Input[i];
                break;
            }
        }

        if (cmdName != "") {

            if (auto Command = FindCommand(cmdName, Parameters); Command != nullptr)
            {
                Command->Parameters = Parameters;
                Command->Activate(this);
            }
            else
            {
                Message_ msg; msg.Username = "Server"; msg.Message = std::format(R"(Command {} was not found!)", cmdName);

                ServerApplication::Object->SendMsgToClient(msg, this->index, false);
            }
        }
        return true;
    }
    return false;
}

User::UserCommand* User::FindCommand(std::string cmdName, std::vector<std::string> Params) {

    if (User::CommandsActive.size() > 0) {


        for (auto currentBlabla = User::CommandsActive.begin(); currentBlabla != User::CommandsActive.end(); currentBlabla++)
        {
            if (currentBlabla->cmdName == cmdName) {
                return currentBlabla._Ptr;
            }
        }
    }

    return nullptr;
}

bool User::UserCommand::Activate(User* user) {
    Commands::UserActivated = user;
    Commands::CommandExecuting = this;

    this->CommandEvent.TriggerEvent();

    Commands::UserActivated = nullptr;
    Commands::CommandExecuting = nullptr;

    return this->WasEventSuccessfull;
}

void ServerApplication::AcceptUsersLoop() {
    while (true)
    {
        int LengthAddr = 0;
        sockaddr AddressOfConnecter;
        int BuffSize = sizeof(sockaddr_in);
        auto ClientSocket = accept(ListenSocket, &AddressOfConnecter, &BuffSize);

        if (ClientSocket == INVALID_SOCKET) {
            std::cout << "[*] Invalid Socket Connection\n";
        }
        else
        {
            auto IP = GetIPString(AddressOfConnecter);
            auto mac = getMacAddress(IP);

            if (mac == "NONE")throw;
            LogServer("User Connected : IP: " + IP + ", MAC_ADDR : " + mac);

            Message_ msgServer;

            msgServer.Username = "Server";
            msgServer.Message = "A User has Connected!";

            User* newUser = new User(IP, mac, ClientSocket);

            SendToClientsMsgs(msgServer, false);



            //ReceiveThreads.push_back(std::thread(ReceiveShitFromClients, (IpsConnected.size() - 1)));


        }
    }
}



std::string ServerApplication::getMacAddress(std::string ipAddress) {
    ULONG macAddr[2];
    ULONG macAddrLen = 6;

    ULONG destinationIpAddr = inet_addr(ipAddress.c_str());

    DWORD ret = SendARP(destinationIpAddr, 0, macAddr, &macAddrLen);

    if (ret == NO_ERROR) {
        //printf("MAC addr : %02X:%02X:%02X:%02X:%02X:%02X\n", ipAddress, static_cast<unsigned char>(macAddr[0] & 0xFF), static_cast<unsigned char>((macAddr[0] >> 8) & 0xFF), static_cast<unsigned char>((macAddr[0] >> 16) & 0xFF), static_cast<unsigned char>((macAddr[0] >> 24) & 0xFF), static_cast<unsigned char>((macAddr[1]) & 0xFF), static_cast<unsigned char>((macAddr[1] >> 8) & 0xFF));

        std::string MacAddr = "";
        {
            MacAddr += std::format("{:x}", static_cast<unsigned char>(macAddr[0] & 0xFF)) + ":";
            MacAddr += std::format("{:x}", static_cast<unsigned char>((macAddr[0] >> 8) & 0xFF)) + ":";
            MacAddr += std::format("{:x}", static_cast<unsigned char>((macAddr[0] >> 16) & 0xFF)) + ":";
            MacAddr += std::format("{:x}", static_cast<unsigned char>((macAddr[0] >> 24) & 0xFF)) + ":";
            MacAddr += std::format("{:x}", static_cast<unsigned char>((macAddr[1]) & 0xFF)) + ":";
            MacAddr += std::format("{:x}", static_cast<unsigned char>((macAddr[1] >> 8) & 0xFF));
        }


        return MacAddr;
    }




    return "NONE";
}

std::string ServerApplication::GetIPString(sockaddr Address) {
    sockaddr_in* addr_in = (sockaddr_in*)&Address;
    std::string IP = inet_ntoa(addr_in->sin_addr);

    return IP;
}

void User::DisconnectClient() 
{
    auto Result = shutdown(this->UserSocket, SD_SEND);

    if (Result == SOCKET_ERROR) {
        closesocket(this->UserSocket);
    }

    this->UserSocket = 0;

    ServerApplication::LogServer("Client Disconnected: IP: " + IP);
}


void User::ReceiveShitFromClients() {
    int iResult;
    do {

        char Buffer[DEFAULT_BUFLEN];
        iResult = recv(this->UserSocket, Buffer, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            struct Message_ UserMessage;

            try
            {
                UserMessage.UnpackMessage(Buffer, iResult);
            }
            catch (const std::exception& e)
            {
                ServerApplication::LogServer(e.what());

                DisconnectClient();

                continue;
            }
            
           

            if (this->Username == "") {

                if (ToLower(UserMessage.Username).find("server") != std::string::npos) {
                    auto Users = User::UsersConnected;

                    int UserNerds = 1;

                    for (size_t i = 0; i < Users.size(); i++)
                    {
                        if (Users[i]->Username.find("Nerd") != std::string::npos) UserNerds++;
                    }

                    UserMessage.Username = "Nerd" + std::to_string(UserNerds);
                }

                {
                    auto Users = User::UsersConnected;
                    int Users_D = 0;

                    for (size_t i = 0; i < Users.size(); i++)
                    {
                        if (Users[i]->Username == UserMessage.Username) {
                            Users_D++;
                            continue;
                        }
                    }

                    if (Users_D > 0) {
                        UserMessage.Username += std::to_string(Users_D);
                    }
                }
                   
                if (UserMessage.Username.size() >= 12) {
                    std::string UserNameOkay = "";

                    for (size_t i = 0; i < 15; i++)
                    {
                        UserNameOkay += UserMessage.Username[i];
                    }

                    UserMessage.Username = UserNameOkay;
                }

                this->SetUsername(UserMessage.Username);

                Message_ msgServer;

                msgServer.Username = "Server";
                msgServer.Message = "";

                ServerApplication::Object->SendMsgToClient(msgServer, this->index, true);

                ServerApplication::LogServer("Handshake completed with: " + this->IP);

                continue;
            }
            else
            {
                UserMessage.Username = this->Username;
                MessageReceived(UserMessage.Message);

                
            }

            if (ActivateCommand(UserMessage.Message)) continue;
            else ServerApplication::Object->SendToClientsMsgs(UserMessage, false);
        }
        else if (iResult == 0)
            printf("Connection closing...\n");
        else {
            if(this->UserSocket != 0)
                DisconnectClient();

            delete this;

            return;
        }

    } while (iResult > 0);

}

void ServerApplication::SendToClientsMsgs(Message_ Message, bool SendKey) {
    Message.EncryptionKey = 20401040;

    Message.EncryptMessage();

    auto packed = Message.PackMessage(false);

    for (size_t i = 0; i < User::UsersConnected.size(); i++)
    {
        auto iSendResult = send(User::UsersConnected[i]->UserSocket, packed, Message.PackedMsgSize, 0);
        if (iSendResult == SOCKET_ERROR) {
            User::UsersConnected[i]->DisconnectClient();
        }
    }
}

void ServerApplication::SendMsgToClient(Message_ Msg__, int Index, bool SendKey) {
    Message_ msg = Msg__;
    msg.EncryptionKey = 20401040;
    msg.EncryptMessage();

    auto packed = msg.PackMessage(SendKey);


    auto iSendResult = send(User::UsersConnected[Index]->UserSocket, packed, msg.PackedMsgSize, 0);
    if (iSendResult == SOCKET_ERROR) {
        closesocket(User::UsersConnected[Index]->UserSocket);
    }
}

std::string ServerApplication::GetOwnIp() {
    std::string IpOut = "";

    char szHostName[255];
    gethostname(szHostName, 255);
    struct hostent* host_entry;

    host_entry = gethostbyname(szHostName);

    for (int i = 0; host_entry->h_addr_list[i] != 0; ++i) {
        struct in_addr addr;
        memcpy(&addr, host_entry->h_addr_list[i], sizeof(struct in_addr));
        IpOut = inet_ntoa(addr);
    }
    
    return IpOut;
}

void User::InitializeCommands() {

    UserCommand kysCommand = { Commands::KysCommand, "kys" };
    UserCommand GetUserCountCommand = { Commands::GetUserCountCommand, "usercount" };
    UserCommand BanUserCommand = { Commands::BanUserCommand, "ban" };

    User::CommandsActive.push_back(kysCommand);
    User::CommandsActive.push_back(GetUserCountCommand);
    User::CommandsActive.push_back(BanUserCommand);
}

void Commands::KysCommand() {
    UserActivated->DisconnectClient();
}

void Commands::GetUserCountCommand() {
    Message_ msg; msg.Username = "Server"; msg.Message = std::format(R"(Current Usercount : {})", User::UsersConnected.size());

    ServerApplication::LogServer(std::format("User Executed: {}", UserActivated->Username));

    ServerApplication::Object->SendMsgToClient(msg, UserActivated->index, false);
}

void Commands::BanUserCommand() {
    Message_ msg;

    if (UserActivated->IP != ServerApplication::Object->OwnIp) {
        msg.Username = UserActivated->Username;
        msg.Message = UserActivated->MessageHistory[UserActivated->MessageHistory.size() - 1];

        ServerApplication::Object->SendToClientsMsgs(msg, false);
    }
    else
    {


        auto Command_ = Commands::CommandExecuting;
        auto& params = Command_->Parameters;

        if (params.size() >= 2)
        {
            std::string UserName = params[0];
            std::string reason = "You were banned for the reason: '";

            for (size_t i = 1; i < params.size(); i++)
            {
                reason += params[i];

                if (i + 1 < params.size())
                    reason += ' ';
            }

            reason += "'";

            User* userToBan = nullptr;

            for (size_t i = 0; i < User::UsersConnected.size(); i++)
            {
                if (User::UsersConnected[i]->Username == UserName) {
                    userToBan = User::UsersConnected[i];
                }
            }

            if (userToBan == nullptr) {
                msg.Username = "Server"; msg.Message = std::format("User {} was not found!", UserName);

                ServerApplication::Object->SendMsgToClient(msg, UserActivated->index, false);
            }
            else
            {
                Message_ message;

                message.Username = "Server";
                message.Message = reason;

                ServerApplication::Object->SendMsgToClient(message, userToBan->index, false);
                userToBan->DisconnectClient();
            }
        }
        else
        {
            msg.Username = "Server"; msg.Message = R"(Not enough Parameters. Parameters are: "Username" "Reason")";

            ServerApplication::Object->SendMsgToClient(msg, UserActivated->index, false);
        }
    }
}

std::vector<User*> User::UsersConnected;
ServerApplication* ServerApplication::Object;
std::vector<User::UserCommand> User::CommandsActive;