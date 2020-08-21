#include "stdafx.h"
#include "CNetWorkLibrary(MemoryPool).h"

BOOL joshua::NetworkLibrary::InitialNetwork(const WCHAR* ip, DWORD port, BOOL Nagle)
{
	int retval;
	bool fail = false;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"WSAStartup() Error!");
		return fail;
	}

	// socket
	_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_listen_socket == INVALID_SOCKET)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"socket() Error!");
		return fail;
	}

	// bind()
	ZeroMemory(&_serveraddr, sizeof(_serveraddr));
	_serveraddr.sin_family = AF_INET;
	if(ip == NULL)
		_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		InetPton(AF_INET, ip, &_serveraddr.sin_addr);
	_serveraddr.sin_port = htons(port);
	retval = bind(_listen_socket, (SOCKADDR*)&_serveraddr, sizeof(_serveraddr));
	if (retval == SOCKET_ERROR)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"bind() Error!");
		return fail;
	}

	//// ���� Send������ ũ�⸦ 0���� ������
	//int optval;

	//int optlen = sizeof(optval);

	//getsockopt(_listen_socket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, &optlen);

	//optval = 0;

	//setsockopt(_listen_socket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));

	//getsockopt(_listen_socket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, &optlen);
	//wprintf(L"sendbuf size : %d\n", optval);

	setsockopt(_listen_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&Nagle, sizeof(Nagle));

	// listen()
	retval = listen(_listen_socket, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"listen() Error!");
		return fail;
	}
	return (!fail);
}

BOOL joshua::NetworkLibrary::CreateThread(DWORD threadCount)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	HANDLE hThread = NULL;
	//  Accept ������ ����
	_ThreadVector.push_back((HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)this, NULL, NULL));
	if (_ThreadVector[0] == NULL)
	{
		return FALSE;
	}
	CloseHandle(_ThreadVector[0]);
	// ������ ����
	if (threadCount == NULL || threadCount > si.dwNumberOfProcessors * 2)
	{
		for (int i = 0; i < si.dwNumberOfProcessors * 2; i++)
		{
			hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)this, 0, NULL);
			if (hThread == NULL)
				return FALSE;
			CloseHandle(hThread);
			_ThreadVector.push_back(hThread);
		}
	}
	else
	{
		for (int i = 0; i < threadCount; i++)
		{
			hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)this, 0, NULL);
			if (hThread == NULL)
				return FALSE;
			CloseHandle(hThread);
			_ThreadVector.push_back(hThread);
		}
	}
	return TRUE;
}

BOOL joshua::NetworkLibrary::CreateSession()
{
	_SessionArray = new st_SESSION[MAX_CLIENT_COUNT];
	for (int i = MAX_CLIENT_COUNT - 1; i >= 0; --i)
	{
		_SessionArray[i].index = i;
		PushIndex(i);
	}
	return TRUE;
}

void joshua::NetworkLibrary::PushIndex(DWORD index)
{
	EnterCriticalSection(&_IndexStackCS);

	_ArrayIndex.push(index);
	if (_ArrayIndex.size() > MAX_CLIENT_COUNT)
	{
		wprintf(L"HERE\n");
	}

	LeaveCriticalSection(&_IndexStackCS);
}

DWORD joshua::NetworkLibrary::PopIndex()
{
	EnterCriticalSection(&_IndexStackCS);

	if (_ArrayIndex.empty())
	{
		LeaveCriticalSection(&_IndexStackCS);
		return -1;
	}
	else
	{
		int temp = _ArrayIndex.top();
		_ArrayIndex.pop();
		LeaveCriticalSection(&_IndexStackCS);

		return temp;
	}
}

DWORD joshua::NetworkLibrary::InsertSession(SOCKET sock, SOCKADDR_IN* sockaddr)
{
	int temp = PopIndex();
	if (temp == -1)
	{
		LOG(L"SERVER", LOG_ERROR, L"IndexStack Lack. CurrentSesionCount : %d\n", _dwSessionCount);
		return temp;
	}
	else
	{
		// TCP ���Ͽ��� ���񼼼� ó��
		// ���� ������ ������������ ����(���� ����, Ȥ�� �ܺο��� ������ �Ϻη� ���� ���)�Ǿ� ������ Ŭ���̾�Ʈ����
		// ������ ������� �𸣴� ������ ������ �ǹ� (Close �̺�Ʈ�� ���� ����)
		// TCP KeepAlive���
				// - SO_KEEPALIVE : �ý��� ������Ʈ�� �� ����. �ý����� ��� SOCKET�� ���ؼ� KEEPALIVE ����
		// - SIO_KEEPALIVE_VALS : Ư�� SOCKET�� KEEPALIVE ����
		tcp_keepalive tcpkl;
		tcpkl.onoff = TRUE;
		tcpkl.keepalivetime = 30000; // ms
		tcpkl.keepaliveinterval = 1000;
		WSAIoctl(sock, SIO_KEEPALIVE_VALS, &tcpkl, sizeof(tcp_keepalive), 0, 0, NULL, NULL, NULL);

		InterlockedIncrement64(&_dwSessionCount);
		InterlockedIncrement64(&_dwCount);		
		_SessionArray[temp].bIsReleased = FALSE;
		_SessionArray[temp].SessionID = InterlockedIncrement64(&_dwSessionID);
		_SessionArray[temp].socket = sock;
		memcpy(&_SessionArray[temp].clientaddr, sockaddr, sizeof(SOCKADDR_IN));
		return temp;
	}
}

unsigned int __stdcall joshua::NetworkLibrary::AcceptThread(LPVOID lpParam)
{
	((NetworkLibrary*)lpParam)->AcceptThread();

	return 0;
}

unsigned int __stdcall joshua::NetworkLibrary::WorkerThread(LPVOID lpParam)
{
	((NetworkLibrary*)lpParam)->WorkerThread();
	return 0;
}

void joshua::NetworkLibrary::AcceptThread(void)
{
	BOOL bFlag = false;
	srand(GetTickCount64() + 1);
	SYSLOGCLASS* _pLog = SYSLOGCLASS::GetInstance();
	while (!bFlag)
	{
		SOCKET clientsocket;
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);

		clientsocket = accept(_listen_socket, (SOCKADDR*)&clientaddr, &addrlen);
		if (clientsocket == INVALID_SOCKET)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_ERROR, L"%s client IP : %s, port : %d\n", L"accept() Error!", szParam, ntohs(clientaddr.sin_port));
			closesocket(clientsocket);
			continue;
		}
		
		if (OnConnectionRequest(&clientaddr) == FALSE)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_WARNNING, L"client IP : %s, port : %d connection denied\n", szParam, ntohs(clientaddr.sin_port));
			closesocket(clientsocket);
			continue;
		}

		if (_dwSessionCount >= _dwSessionMax)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_WARNNING, L"client IP : %s, port : %d connection denied, MaxClient Over\n", szParam, ntohs(clientaddr.sin_port));
			closesocket(clientsocket);
			continue;
		}

		DWORD index = InsertSession(clientsocket, &clientaddr);
		if (index == -1)
		{
			// ���� ������
			// TODO dump���� ����
			closesocket(clientsocket);
			continue;
		}


		// ���ϰ� ����� �Ϸ� ��Ʈ ����
		if (CreateIoCompletionPort((HANDLE)clientsocket, (HANDLE)_hCP, (ULONG_PTR)&_SessionArray[index], 0) == NULL)
		{
			//  HANDLE ���ڿ� ������ �ƴ� ���� �� ��� �߸��� �ڵ�(6�� ����) �߻�
			// ������ �ƴ� ���� �־��ٴ� ���� �ٸ� �����忡�� ������ ��ȯ�ߴٴ� �ǹ��̹Ƿ� ����ȭ ������ ���ɼ��� ����.
			LOG(L"SYSTEM", LOG_ERROR, L"AcceptThread() - CreateIoCompletionPort() failed%d", WSAGetLastError());
			if (InterlockedDecrement(&_SessionArray[index].dwIOCount) == 0)
				SessionRelease(&_SessionArray[index]);
		}

		OnClientJoin(&_SessionArray[index].clientaddr, _SessionArray[index].SessionID);

	
		PostRecv(&_SessionArray[index]);
	}
	return;
}

void joshua::NetworkLibrary::WorkerThread(void)
{
	int retval;
	// �񵿱� ����� �Ϸ� ��ٸ���
	DWORD cbTransferred;
	LPOVERLAPPED pOverlapped;
	st_SESSION* pSession = nullptr;
	while (true)
	{
		cbTransferred = 0;
		pOverlapped = 0;
		pSession = nullptr;

		// GQCS return ����� ��.
		// 1. IOCP Queue�κ��� �Ϸ���Ŷ�� ���� �� ������ ��� => TRUE ����, lpCompletionKey ����

		// 2. IOCP Queue�κ��� �Ϸ���Ŷ�� ���� �� ������ ���� ���ÿ� lpOverlapped�� NULL�� ��� => FALSE ����

		// 3. IOCP Queue�κ��� �Ϸ���Ŷ�� ���� �� ������ ���� ���ÿ� 
		//	  lpOverlapped�� NULL�� �ƴ� ����̸鼭 
		//    dequeue�� ��Ұ� ������ I/O�� ���
		// => FALSE����, lpCompletionKey ����

		// 4. IOCP�� ��ϵ� ������ close�� ���
		// => FALSE ����, lpOverlapped�� NULL�� �ƴϰ�,
		// => lpNumberOfBytes�� 0 ����.

		// 5. �������� (PQCS)
		// => lpCompletionKey�� NULL, lpOverlapped�� NULL, lpNumberOfBytes���� NULL

		retval = GetQueuedCompletionStatus(_hCP, &cbTransferred, reinterpret_cast<PULONG_PTR>(&pSession), (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (pOverlapped == NULL)
		{
			// 2��.
			if (retval == FALSE)
			{
				LOG(L"SYSTEM", LOG_ERROR, L"GetQueuedCompletionStatus() failed %d", GetLastError());
				PostQueuedCompletionStatus(_hCP, 0, 0, 0);	// GQCS ���� �� ��ġ�� ���Ҹ��Ѱ� �����Ƿ� ����
				break;
			}

			// ��������
			if (pSession == NULL && cbTransferred == NULL)
			{
				PostQueuedCompletionStatus(_hCP, 0, 0, 0);	// GQCS ���� �� ��ġ�� ���Ҹ��Ѱ� �����Ƿ� ����
				break;
			}
		}

		if (cbTransferred == 0)
		{
			shutdown(pSession->socket, SD_BOTH);
		}
		else
		{
			if (pOverlapped == &pSession->RecvOverlapped)
			{
				RecvComplete(pSession, cbTransferred);
			}
			if (pOverlapped == &pSession->RecvOverlapped)
			{
				SendComplete(pSession, cbTransferred);
			}
		}

		if (InterlockedDecrement(&pSession->dwIOCount) == 0)
		{
			SessionRelease(pSession);
		}
	}
	return;
}

bool joshua::NetworkLibrary::PostSend(st_SESSION* session)
{
	while (true)
	{
		// �� ���� �����Ͱ� ���� ���
		if (session->SendBuffer.GetUseSize() == 0)
			return false;
		// send flag�� ������ �ȵ� ���
		if (InterlockedCompareExchange(&session->bIsSend, TRUE, FALSE) == TRUE)
			return false;

		// ���� �����Ͱ� �־����� �ٸ� �����忡�� �Ϸ�������
		if (session->SendBuffer.GetUseSize() == 0)
		{
			InterlockedExchange(&session->bIsSend, FALSE);
			continue;
		}
		break;
	}

	WSABUF wsabuf[1000];

	int iUsingSize = session->SendBuffer.GetUseSize();
	int i = 0;
	while (true)
	{
		CMessage* pPacket = nullptr;

		if (iUsingSize == 0 || iUsingSize < 8)
			break;

		if (session->SendBuffer.Get((char*)&pPacket, 8) != 8)
			break;

		session->lMessageList.push_back(pPacket);

		wsabuf[i].buf = pPacket->GetBufferPtr();
		wsabuf[i].len = pPacket->GetDataSize();
		i++;
		iUsingSize -= 8;
	}
	session->dwPacketCount = i;

	DWORD dwTransferred;
	ZeroMemory(&session->SendOverlapped, sizeof(OVERLAPPED));
	InterlockedIncrement(&session->dwIOCount);
	if (WSASend(session->socket, wsabuf, i, &dwTransferred, 0, &session->SendOverlapped, NULL) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();

		// WSASend�� pending�� �ɸ� ���� ���� send �ӽ� ���۰� ������ ����
		if (error != WSA_IO_PENDING)
		{
			if(error != 10038 && error != 10053 && error != 10054 && error != 10058)
				LOG(L"SYSTEM", LOG_ERROR, L"WSASend() # failed%d / Socket:%d / IOCnt:%d / SendQ Size:%d", error, session->socket, session->dwIOCount, session->SendBuffer.GetUseSize());

			shutdown(session->socket, SD_BOTH);
			if (InterlockedDecrement(&session->dwIOCount) == 0)
				SessionRelease(session);
		}
	}

	return true;
}

bool joshua::NetworkLibrary::PostRecv(st_SESSION* pSession)
{
	WSABUF wsabuf[2];
	int iBufCnt = 1;
	wsabuf[0].buf = pSession->RecvBuffer.GetWriteBufferPtr();
	wsabuf[0].len = pSession->RecvBuffer.GetNotBrokenPutSize();

	if (pSession->RecvBuffer.GetNotBrokenPutSize() < pSession->RecvBuffer.GetFreeSize())
	{
		wsabuf[1].buf = pSession->RecvBuffer.GetBufferPtr();
		wsabuf[1].len = pSession->RecvBuffer.GetFreeSize() - wsabuf[0].len;
		++iBufCnt;
	}

	DWORD dwTransferred = 0;
	DWORD dwFlag = 0;
	ZeroMemory(&pSession->RecvOverlapped, sizeof(pSession->RecvOverlapped));
	InterlockedIncrement(&pSession->dwIOCount);
	if (WSARecv(pSession->socket, wsabuf, iBufCnt, &dwTransferred, &dwFlag, &pSession->RecvOverlapped, NULL) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();

		// WSASend�� pending�� �ɸ� ���� ���� send �ӽ� ���۰� ������ ����
		if (error != WSA_IO_PENDING)
		{			// WSAENOTSOCK(10038) : ������ �ƴ� �׸� ���� �۾� �õ�
			// WSAECONNABORTED(10053) : ȣ��Ʈ�� ���� ����. ������ ���� �ð� �ʰ�, �������� ���� �߻�
			// WSAECONNRESET(10054) : ���� ȣ��Ʈ�� ���� ���� ���� ���� ����. ���� ȣ��Ʈ�� ���ڱ� �����ǰų� �ٽ� ���۵ǰų� �ϵ� ���Ḧ ����ϴ� ���
			// WSAESHUTDOWN(10058) : ���� ���� �� ����
			if (error != 10038 && error != 10053 && error != 10054 && error != 10058)
				LOG(L"SYSTEM", LOG_ERROR, L"WSASend() # failed%d / Socket:%d / IOCnt:%d / SendQ Size:%d", error, pSession->socket, pSession->dwIOCount, pSession->SendBuffer.GetUseSize());

			shutdown(pSession->socket, SD_BOTH);
			if (InterlockedDecrement(&pSession->dwIOCount) == 0)
				SessionRelease(pSession);
		}
	}

	return true;
}


BOOL joshua::NetworkLibrary::Start(DWORD port, BOOL nagle, const WCHAR* ip, DWORD threadCount, DWORD MaxClient)
{
	SYSLOGCLASS* _pLog = SYSLOGCLASS::GetInstance();
	// ���� �ʱ�ȭ
	_dwSessionMax = MaxClient;

	if (InitialNetwork(ip, port, nagle) == false)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"Initialize failed!");
		return FALSE;
	}

	// IOCP ����
	_hCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_hCP == NULL)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"CreateIoCompletionPort() ERROR");
		return FALSE;
	}

	// ������ ����
	if (CreateThread(threadCount) == FALSE)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"CreateThread() ERROR");
		return FALSE;
	}

	// ���� �Ҵ� �� �ε��� ���� ����
	if (CreateSession() == FALSE)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"CreateSession() ERROR");
		return FALSE;
	}

	_bServerOn = TRUE;
	return TRUE;
}

void joshua::NetworkLibrary::SessionRelease(st_SESSION* pSession)
{
	pSession->socket = INVALID_SOCKET;
	ZeroMemory(&pSession->clientaddr, sizeof(SOCKADDR_IN));
	if (pSession->SendBuffer.GetUseSize() > 0)
	{
		while (pSession->SendBuffer.GetUseSize() > 0)
		{
			CMessage* packet;
			pSession->SendBuffer.Get((char*)&packet, 8);
			pSession->lMessageList.push_back(packet);
		}
	}
	if (pSession->lMessageList.size() != 0)
	{
		for (std::list<CMessage*>::iterator itor = pSession->lMessageList.begin(); itor != pSession->lMessageList.end();)
		{
			(*itor)->SubRef();
			itor = pSession->lMessageList.erase(itor);
		}
	}


	pSession->RecvBuffer.ClearBuffer();
	pSession->SendBuffer.ClearBuffer();
	pSession->dwPacketCount = 0;
	pSession->SessionID = -1;
	pSession->dwIOCount = 0;
	pSession->bIsSend = FALSE;
	pSession->bIsReleased = TRUE;
	//InterlockedDecrement64(&_dwSessionCount);
	PushIndex(pSession->index);
}

void joshua::NetworkLibrary::SendPacket(LONG64 id, CMessage* message)
{
	st_SESSION* pSession = SessionReleaseCheck(id);
	if (pSession == nullptr)
		return;

	message->AddRef();
	pSession->SendBuffer.Put((char*)&message, 8);
	PostSend(pSession);

	if (InterlockedDecrement(&pSession->dwIOCount) == 0)
		SessionRelease(pSession);
	return;
}

void joshua::NetworkLibrary::PrintPacketCount()
{
	wprintf(L"SessionCount : %ld, Allock Count : %d\n", _dwSessionCount, CMessage::g_PacketPool->GetAllocCount());
}

void joshua::NetworkLibrary::DisconnectSession(DWORD id)
{
	LINGER optval;
	int retval;
	optval.l_onoff = 1;
	optval.l_linger = 0;

	int index = 0;
	for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	{
		if (id == _SessionArray[i].SessionID)
		{
			index = i;
			break;
		}
	}
	retval = setsockopt(_SessionArray[index].socket, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
	closesocket(_SessionArray[index].socket);

}

joshua::st_SESSION* joshua::NetworkLibrary::SessionReleaseCheck(LONG64 iSessionID)
{
	// multi-thread ȯ�濡�� release, connectm send, accept ���� ���ÿ� �߻��� �� ������ �����ؾ� �Ѵ�.
	st_SESSION* pSession = nullptr;

	for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	{
		if (_SessionArray[i].SessionID == iSessionID)
		{
			pSession = &_SessionArray[i];
			break;
		}
	}

	// 1. ReleaseFlag�� Ȯ���ϰ� ����� ����
	if (pSession->bIsReleased == TRUE)
		return nullptr;

	// 2. ReleaseSession �Լ� ���� �ִ� ���
	// �̶� �ٸ� �����忡�� SendPacket�̳� Disconnect �õ� ��, IOCOUNT�� 1�� �� �� ����
	if (InterlockedIncrement(&pSession->dwIOCount) == 1)
	{
		if (InterlockedDecrement(&pSession->dwIOCount) == 0)
			SessionRelease(pSession);

		return nullptr;
	}

	// 3. �̹� disconnect �� ���� accept�Ͽ� �� session���� ��ü�� ���
	if (pSession->SessionID != iSessionID)
	{
		if(InterlockedDecrement(&pSession->dwIOCount) == 0)
			SessionRelease(pSession);
		return nullptr;
	}

	// 4. releaseFlag�� FALSE�� ���
	if (pSession->bIsReleased == FALSE)
		return pSession;

	// 5. 4�� �����ϱ� ������ TRUE�� �� ���
	if (InterlockedIncrement(&pSession->dwIOCount) == 0)
		SessionRelease(pSession);
	return nullptr;
}

void joshua::NetworkLibrary::RecvComplete(st_SESSION* pSession, DWORD dwTransferred)
{	// ���ŷ���ŭ RecvQ.MoveWritePos �̵�
	pSession->RecvBuffer.MoveWritePos(dwTransferred);

	// Header�� Payload ���̸� ���� 2Byte ũ���� Ÿ��
	WORD wHeader;
	while (1)
	{
		// 1. RecvQ�� 'sizeof(Header)'��ŭ �ִ��� Ȯ��
		int iRecvSize = pSession->RecvBuffer.GetUseSize();
		if (iRecvSize < sizeof(wHeader))
		{
			// �� �̻� ó���� ��Ŷ�� ����
			// ����, Header ũ�⺸�� ���� ������ �����Ͱ� �����ȴٸ� 2������ �ɷ���
			break;
		}

		// 2. Packet ���� Ȯ�� : Headerũ��('sizeof(Header)') + Payload����('Header')
		pSession->RecvBuffer.Peek(reinterpret_cast<char*>(&wHeader), sizeof(wHeader));
		if (iRecvSize < sizeof(wHeader) + wHeader)
		{
			// Header�� Payload�� ���̰� ������ �ٸ�
			shutdown(pSession->socket, SD_BOTH);
			LOG(L"SYSTEM", LOG_WARNNING, L"Header & PayloadLength mismatch");
			break;
		}
		// RecvQ���� Packet�� Header �κ� ����
		pSession->RecvBuffer.MoveWritePos(sizeof(wHeader));

		CMessage* pPacket = CMessage::Alloc();

		// 3. Payload ���� Ȯ�� : PacketBuffer�� �ִ� ũ�⺸�� Payload�� Ŭ ���
		if (pPacket->GetBufferSize() < wHeader)
		{
			pPacket->SubRef();
			shutdown(pSession->socket, SD_BOTH);
			LOG(L"SYSTEM", LOG_WARNNING, L"PacketBufferSize < PayloadSize ");
			break;
		}

		// 4. PacketPool�� Packet ������ �Ҵ�
		if (pSession->RecvBuffer.Get(pPacket->GetBufferPtr(), wHeader) != wHeader)
		{
			pPacket->SubRef();
			LOG(L"SYSTEM", LOG_WARNNING, L"RecvQ dequeue error");
			shutdown(pSession->socket, SD_BOTH);
			break;
		}

		// RecvQ���� Packet�� Payload �κ� ����
		pPacket->MoveWritePos(wHeader);

		// 5. Packet ó��
		OnRecv(pSession->SessionID, pPacket);
		pPacket->SubRef();
	}
	PostRecv(pSession);
}

void joshua::NetworkLibrary::SendComplete(st_SESSION* pSession, DWORD dwTransferred)
{
	OnSend(pSession->SessionID, dwTransferred);

	for (std::list<CMessage*>::iterator itor = pSession->lMessageList.begin(); itor != pSession->lMessageList.end();)
	{
		(*itor)->SubRef();
		itor = pSession->lMessageList.erase(itor);
	}

	if (pSession->bIsSendDisconnect == TRUE)
		shutdown(pSession->socket, SD_BOTH);

	InterlockedExchange(&pSession->bIsSend, FALSE);
	PostSend(pSession);
}
