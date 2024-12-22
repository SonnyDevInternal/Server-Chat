#include "Server.h"

int main() {
	ServerApplication* Server = new ServerApplication();

	User::InitializeCommands();

	return Server->MainServer();
}