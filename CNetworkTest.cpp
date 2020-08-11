#include "stdafx.h"
#include "CNetworkTest.h"

void Network::OnClientJoin(SOCKADDR_IN* sockAddr, DWORD sessionID)
{
}

void Network::OnClientLeave(DWORD sessionID)
{
}

bool Network::OnConnectionRequest(SOCKADDR_IN* sockAddr)
{
    return true;
}

void Network::OnRecv(DWORD sessionID, CMessage* message)
{
	SendPacket(sessionID, message);
}

void Network::OnSend(DWORD sessionID, int sendsize)
{
}

void Network::OnError(int errorcode, WCHAR*)
{
}
