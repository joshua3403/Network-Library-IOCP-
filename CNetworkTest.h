#pragma once
#include "stdafx.h"
#include "CNetWorkLibrary(MemoryPool).h"

class Network : public joshua::NetworkLibrary
{
public:

public:
	void OnClientJoin(SOCKADDR_IN* sockAddr, DWORD sessionID);
	virtual void OnClientLeave(DWORD sessionID);

	// accept����
	// TRUE�� ���� ���
	// FALSE�� ���� ����
	bool OnConnectionRequest(SOCKADDR_IN* sockAddr);

	void OnRecv(DWORD sessionID, CMessage* message);
	void OnSend(DWORD sessionID, int sendsize);

	//	virtual void OnWorkerThreadBegin() = 0;                    
	//	virtual void OnWorkerThreadEnd() = 0;                      

	void OnError(int errorcode, WCHAR*);
};