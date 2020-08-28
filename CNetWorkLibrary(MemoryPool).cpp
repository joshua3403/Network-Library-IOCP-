#include "stdafx.h"
#include "CCrashDumpClass.h"
#include "CNetWorkLibrary(MemoryPool).h"

BOOL joshua::NetworkLibrary::InitialNetwork(const WCHAR* ip, DWORD port, BOOL Nagle)
{
	int retval;
	bool fail = false;
	_bNagle = Nagle;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"WSAStartup() Error!");
		return fail;
	}

	// socket
	_slisten_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_slisten_socket == INVALID_SOCKET)
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
	retval = bind(_slisten_socket, (SOCKADDR*)&_serveraddr, sizeof(_serveraddr));
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

	setsockopt(_slisten_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&Nagle, sizeof(Nagle));

	// listen()
	retval = listen(_slisten_socket, SOMAXCONN);
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
	_AcceptThread = ((HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)this, NULL, NULL));
	if (_AcceptThread == NULL)
	{
		return FALSE;
	}
	CloseHandle(_AcceptThread);
	// ������ ����
	_iThreadCount = threadCount;
	if (_iThreadCount == NULL || _iThreadCount > si.dwNumberOfProcessors * 2)
	{
		for (int i = 0; i < si.dwNumberOfProcessors * 2; i++)
		{
			hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)this, 0, NULL);
			if (hThread == NULL)
				return FALSE;
			CloseHandle(hThread);
			_ThreadVector.push_back(hThread);
		}
		_iThreadCount = si.dwNumberOfProcessors * 2;
	}
	else
	{
		for (int i = 0; i < _iThreadCount; i++)
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
	_SessionArray = new st_SESSION[_dwSessionMax];
	for(__int64 i = _dwSessionMax - 1; i >= 0; --i)
	{
		_SessionArray[i].index = i;
		PushIndex(i);
	}
	return TRUE;
}

void joshua::NetworkLibrary::PushIndex(UINT64 index)
{
	EnterCriticalSection(&_IndexStackCS);
	_ArrayIndex.push(index);
	LeaveCriticalSection(&_IndexStackCS);
}

UINT64 joshua::NetworkLibrary::PopIndex()
{
	EnterCriticalSection(&_IndexStackCS);
	UINT64 temp = _ArrayIndex.top();
	_ArrayIndex.pop();
	LeaveCriticalSection(&_IndexStackCS);

	return temp;
 
}

joshua::st_SESSION* joshua::NetworkLibrary::InsertSession(SOCKET sock, SOCKADDR_IN* sockaddr)
{
	UINT64 temp = -1;
	temp = PopIndex();
	if (temp == -1)
	{
		LOG(L"SERVER", LOG_ERROR, L"IndexStack Lack. CurrentSesionCount : %d\n", _dwSessionCount);
		return nullptr;
	}
	else
	{
		// TCP ���Ͽ��� ���񼼼� ó��
		// ���� ������ ������������ ����(���� ����, Ȥ�� �ܺο��� ������ �Ϻη� ���� ���)�Ǿ� ������ Ŭ���̾�Ʈ����
		// ������ ������� �𸣴� ������ ������ �ǹ� (Close �̺�Ʈ�� ���� ����)
		// TCP KeepAlive���
				// - SO_KEEPALIVE : �ý��� ������Ʈ�� �� ����. �ý����� ��� SOCKET�� ���ؼ� KEEPALIVE ����
		// - SIO_KEEPALIVE_VALS : Ư�� SOCKET�� KEEPALIVE ����
		//tcp_keepalive tcpkl;
		//tcpkl.onoff = TRUE;
		//tcpkl.keepalivetime = 30000; // ms
		//tcpkl.keepaliveinterval = 1000;
		//WSAIoctl(sock, SIO_KEEPALIVE_VALS, &tcpkl, sizeof(tcp_keepalive), 0, 0, NULL, NULL, NULL);
	/*	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&_bNagle, sizeof(_bNagle));*/

		st_SESSION* pSession = &_SessionArray[temp];
		pSession->SessionID = CreateSessionID(++_dwSessionID, temp);
		pSession->socket = sock;
		pSession->bIsSend = FALSE;
		pSession->dwPacketCount = 0;
		pSession->SendBuffer.ClearBuffer();
		pSession->RecvBuffer.ClearBuffer();
		pSession->lIO->lIOCount = 0;
		pSession->lIO->bIsReleased = FALSE;
		InterlockedIncrement64(&pSession->lIO->lIOCount);
		memcpy(&pSession->clientaddr, sockaddr, sizeof(SOCKADDR_IN));
		InterlockedIncrement64(&_dwSessionCount);
		InterlockedIncrement64(&_dwCount);
		return pSession;
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

		clientsocket = accept(_slisten_socket, reinterpret_cast<sockaddr*>(&clientaddr), &addrlen);

		_lAcceptCount++;
		_lAcceptTPS++;

		if (!_bServerOn)
			break;
		//clientsocket = accept(_slisten_socket, (SOCKADDR*)&clientaddr, &addrlen);
		if (clientsocket == INVALID_SOCKET)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_ERROR, L"%s client IP : %s, port : %d\n", L"accept() Error!", szParam, ntohs(clientaddr.sin_port));
			DisconnectSocket(clientsocket);
			continue;
		}
		
		if (OnConnectionRequest(&clientaddr) == FALSE)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_WARNNING, L"client IP : %s, port : %d connection denied\n", szParam, ntohs(clientaddr.sin_port));
			DisconnectSocket(clientsocket);
			continue;
		}

		if (_dwSessionCount >= _dwSessionMax)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_WARNNING, L"client IP : %s, port : %d connection denied, MaxClient Over\n", szParam, ntohs(clientaddr.sin_port));
			DisconnectSocket(clientsocket);
			continue;
		}
		st_SESSION* pSession = InsertSession(clientsocket, &clientaddr);
		if (pSession == nullptr)
		{
			// ���� ������
			// TODO dump���� ����
			InterlockedDecrement64(&_dwSessionCount);
			DisconnectSocket(clientsocket);
			continue;
		}
		// ���ϰ� ����� �Ϸ� ��Ʈ ����
		if (CreateIoCompletionPort((HANDLE)pSession->socket, (HANDLE)_hCP, (ULONG_PTR)pSession, 0) == NULL)
		{
			//  HANDLE ���ڿ� ������ �ƴ� ���� �� ��� �߸��� �ڵ�(6�� ����) �߻�
			// ������ �ƴ� ���� �־��ٴ� ���� �ٸ� �����忡�� ������ ��ȯ�ߴٴ� �ǹ��̹Ƿ� ����ȭ ������ ���ɼ��� ����.
			LOG(L"SYSTEM", LOG_ERROR, L"AcceptThread() - CreateIoCompletionPort() failed : %d", WSAGetLastError());
			if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
			{
				DisconnectSession(pSession);
				SessionRelease(pSession);
				continue;
			}
		}
		pSession->lIO->bIsReleased = FALSE;

		OnClientJoin(&pSession->clientaddr, pSession->SessionID);
		
		PostRecv(pSession);


		if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
		{
			SessionRelease(pSession);
		}
	
	}
	LOG(L"SYSTEM", LOG_DEBUG, L"AcceptThread Exit");
	return;
}

void joshua::NetworkLibrary::WorkerThread(void)
{
	int retval;
	// �񵿱� ����� �Ϸ� ��ٸ���
	DWORD cbTransferred;
	LPOVERLAPPED pOverlapped;
	st_SESSION* pSession;
	while (true)
	{
		cbTransferred = 0;
		pOverlapped = 0;
		pSession = 0;


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

		retval = GetQueuedCompletionStatus(_hCP, &cbTransferred, reinterpret_cast<PULONG_PTR>(&pSession), &pOverlapped, INFINITE);

		if (pOverlapped == 0)
		{
			// 2��.
			if (retval == FALSE)
			{
				LOG(L"SYSTEM", LOG_ERROR, L"GetQueuedCompletionStatus() failed %d", GetLastError());
				PostQueuedCompletionStatus(_hCP, 0, 0, 0);	// GQCS ���� �� ��ġ�� ���Ҹ��Ѱ� �����Ƿ� ����
				break;
			}

			// ��������
			if (pSession == 0 && cbTransferred == 0)
			{
				PostQueuedCompletionStatus(_hCP, 0, 0, 0);	// GQCS ���� �� ��ġ�� ���Ҹ��Ѱ� �����Ƿ� ����
				break;
			}
		}

		if (cbTransferred == 0)
		{
			DisconnectSession(pSession);
		}
		else
		{
			if (pOverlapped == &pSession->RecvOverlapped)
			{
				RecvComplete(pSession, cbTransferred);
				//wprintf(L"Recv Session IO Count : %d, len : %d\n", pSession->dwIOCount, cbTransferred);
			}
			if (pOverlapped == &pSession->SendOverlapped)
			{
				SendComplete(pSession, cbTransferred);
				//wprintf(L"Send Session IO Count : %d, len : %d\n", pSession->dwIOCount, cbTransferred);
			}
		}

		//wprintf(L"Final Session IO Count : %d\n", pSession->dwIOCount);

		if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
		{
			SessionRelease(pSession);
		}
	}
	return;
}

bool joshua::NetworkLibrary::PostSend(st_SESSION* pSession)
{
	while (1)
	{
		if (pSession->SendBuffer.GetUseSize() == 0)
			return false;
		if (InterlockedCompareExchange(&pSession->bIsSend, TRUE, FALSE) == TRUE)
			return false;
		if (pSession->SendBuffer.GetUseSize() == 0)
		{
			InterlockedExchange(&pSession->bIsSend, FALSE);
			continue;
		}
		break;
	}

	WSABUF wsaBuf[1000];

	int i = 0; 
	int usingSize = pSession->SendBuffer.GetUseSize();
	while (usingSize > 0)
	{
		if (usingSize < 8)
			break;
		CMessage* packet;
		pSession->SendBuffer.Get((char*)&packet, 8);
		wsaBuf[i].buf = packet->GetBufferPtr();
		wsaBuf[i].len = packet->GetDataSize();
		pSession->lMessageList.push_back(packet);
		i++;
		usingSize -= 8;
	}
	pSession->dwPacketCount = i;
	DWORD dwTransferred = 0;
	ZeroMemory(&pSession->SendOverlapped, sizeof(OVERLAPPED));
	InterlockedIncrement64(&pSession->lIO->lIOCount);
	if (WSASend(pSession->socket, wsaBuf, i, &dwTransferred, 0, &pSession->SendOverlapped, NULL) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();

		// WSASend�� pending�� �ɸ� ���� ���� send �ӽ� ���۰� ������ ����
		if (error != WSA_IO_PENDING)
		{
			if (error != 10038 && error != 10053 && error != 10054 && error != 10058)
				LOG(L"SYSTEM", LOG_ERROR, L"WSASend() # failed%d / Socket:%d / IOCnt:%d / SendQ Size:%d", error, pSession->socket, pSession->lIO->lIOCount, pSession->SendBuffer.GetUseSize());

			//DisconnectSession(pSession);
			if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
				SessionRelease(pSession);

			return false;
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
	ZeroMemory(&pSession->RecvOverlapped, sizeof(OVERLAPPED));
	InterlockedIncrement64(&pSession->lIO->lIOCount);
	//wprintf(L"IO COUNT : %d\n", pSession->dwIOCount);
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
				LOG(L"SYSTEM", LOG_ERROR, L"WSASend() # failed%d / Socket:%d / IOCnt:%d / SendQ Size:%d", error, pSession->socket, pSession->lIO->lIOCount, pSession->SendBuffer.GetUseSize());

			//DisconnectSession(pSession);
			if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
				SessionRelease(pSession);

			return false;

		}
	}

	return true;
}


BOOL joshua::NetworkLibrary::Start(DWORD port, BOOL nagle, const WCHAR* ip, DWORD threadCount, __int64 MaxClient)
{
	SYSLOGCLASS* _pLog = SYSLOGCLASS::GetInstance();
	// ���� �ʱ�ȭ
	_dwSessionMax = MaxClient;
	_bServerOn = TRUE;

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

	return TRUE;
}

void joshua::NetworkLibrary::SessionRelease(st_SESSION* pSession)
{
	st_SESSION_FLAG temp(0, FALSE);
	if (!InterlockedCompareExchange128((LONG64*)pSession->lIO, TRUE, 0, (LONG64*)&temp))
		return;

	//DisconnectSession(pSession);

	OnClientLeave(pSession->SessionID);

	ZeroMemory(&pSession->clientaddr, sizeof(SOCKADDR_IN));
	if (pSession->SendBuffer.GetUseSize() > 0)
	{
		while (pSession->SendBuffer.GetUseSize() > 0)
		{
			CMessage* packet;
			pSession->SendBuffer.Get((char*)&packet, 8);
			pSession->lMessageList.push_back(packet);
		}
		if (pSession->lMessageList.size() != 0)
		{
			for (std::list<CMessage*>::iterator itor = pSession->lMessageList.begin(); itor != pSession->lMessageList.end();)
			{
				(*itor)->SubRef();
				itor = pSession->lMessageList.erase(itor);
			}
		}
	}

	InterlockedExchange(&pSession->bIsSend, FALSE);
	pSession->SessionID = -1;
	pSession->socket = INVALID_SOCKET;
	//pSession->RecvBuffer.ClearBuffer();
	//pSession->SendBuffer.ClearBuffer();
	pSession->dwPacketCount = 0;
	pSession->lMessageList.clear();
	PushIndex(pSession->index);
	InterlockedDecrement64(&_dwSessionCount);

}

void joshua::NetworkLibrary::DisconnectSocket(SOCKET sock)
{
	LINGER optval;
	int retval;
	optval.l_onoff = 1;
	optval.l_linger = 0;

	retval = setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
	closesocket(sock);
}

void joshua::NetworkLibrary::SendPacket(UINT64 id, CMessage* message)
{
	st_SESSION* pSession = SessionReleaseCheck(id);
	if (pSession == nullptr)
		return;

	message->AddRef();
	pSession->SendBuffer.Put((char*)&message, 8);

	PostSend(pSession);

	if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
		SessionRelease(pSession);

	return;
}

void joshua::NetworkLibrary::Stop()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	_bServerOn = FALSE;

	closesocket(_slisten_socket);

	WaitForSingleObject(_AcceptThread, INFINITE);

	for (int i = 0; i < si.dwNumberOfProcessors * 2; i++)
	{
		PostQueuedCompletionStatus(_hCP, 0, 0, 0);
	}

	WaitForMultipleObjects(_iThreadCount, (HANDLE*)&_ThreadVector, TRUE, INFINITE);

	WSACleanup();

	CloseHandle(_hCP);

	for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	{
		if (_SessionArray[i].socket != INVALID_SOCKET)
			SessionRelease(&_SessionArray[i]);
	}

	if (_SessionArray != NULL)
	{
		delete[] _SessionArray;
		_SessionArray = NULL;
	}

	_slisten_socket = INVALID_SOCKET;
	_dwSessionID = 0;
	_dwSessionCount = _dwSessionMax = 0;
	_hCP = INVALID_HANDLE_VALUE;
	ZeroMemory(&_serveraddr, sizeof(_serveraddr));
	_bNagle = FALSE;
	_dwCount = 0;
	_iThreadCount = 0;
	_ThreadVector.clear();
	for (int i = 0; i < _ArrayIndex.size(); i++)
	{
		_ArrayIndex.pop();
	}
}

void joshua::NetworkLibrary::PrintPacketCount()
{
	wprintf(L"==================== IOCP Echo Server Test ====================\n");
	wprintf(L" - SessionCount : %ld\n", _dwSessionCount);
	wprintf(L" - Accept Count : %08lld\n", _lAcceptCount);
	wprintf(L" - Accept TPS : %08lld\n", _lAcceptTPS);
	wprintf(L" - Send TPS : %08lld\n", _lSendTPS);
	wprintf(L" - Recv TPS : %08lld\n", _lRecvTPS);
	wprintf(L"===============================================================\n");
	_lAcceptTPS = _lSendTPS = _lRecvTPS = 0;

}

void joshua::NetworkLibrary::DisconnectSession(st_SESSION* pSession)
{
	LINGER optval;
	int retval;
	optval.l_onoff = 1;
	optval.l_linger = 0;

	retval = setsockopt(pSession->socket, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
	closesocket(pSession->socket);

}

joshua::st_SESSION* joshua::NetworkLibrary::SessionReleaseCheck(UINT64 iSessionID)
{
	// multi-thread ȯ�濡�� release, connectm send, accept ���� ���ÿ� �߻��� �� ������ �����ؾ� �Ѵ�.
	int nIndex = GetSessionIndex(iSessionID);
	st_SESSION* pSession = &_SessionArray[nIndex];
	//st_SESSION* pSession = nullptr;
	//for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	//{
	//	if (_SessionArray[i].SessionID == iSessionID)
	//	{
	//		pSession = &_SessionArray[i];
	//		break;
	//	}
	//}

	// �̹� ���� �Ǿ��ٸ�(�̹� ���� �ڵ带 ���ٸ�)
	if (pSession->lIO->bIsReleased == TRUE)
		return nullptr;

	// ioCount�� �����ؼ� 1�̶�� ���� ��𼱰� �� ���ǿ� ���� release�� ����ǰ� �ִٴ� ��.
	if (InterlockedIncrement64(&pSession->lIO->lIOCount) == 1) 
	{
		if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
			SessionRelease(pSession);

		return nullptr;
	}

	// ������ �˻��ؼ� ������� �̹� �ٸ� �������� ��ü�� ���
	if (pSession->SessionID != iSessionID)
	{
		if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
		{
			SessionRelease(pSession);
		}
		return nullptr;
	}

	if (pSession->lIO->bIsReleased == FALSE)
		return pSession;

	if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
		SessionRelease(pSession);

	//if (pSession == nullptr)
	//	return nullptr;


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

		if(wHeader != 8)
		{			// Header�� Payload�� ���̰� ������ �ٸ�
			DisconnectSession(pSession);
			LOG(L"SYSTEM", LOG_WARNNING, L"Header Error");
			break;
		}

		if (iRecvSize < sizeof(wHeader) + wHeader)
		{
			// Header�� Payload�� ���̰� ������ �ٸ�
			DisconnectSession(pSession);
			LOG(L"SYSTEM", LOG_WARNNING, L"Header & PayloadLength mismatch");
			break;
		}

		CMessage* pPacket = CMessage::Alloc();

		(*pPacket) << wHeader;

		//// 3. Payload ���� Ȯ�� : PacketBuffer�� �ִ� ũ�⺸�� Payload�� Ŭ ���
		//if (pPacket->GetBufferSize() < wHeader)
		//{
		//	pPacket->SubRef();
		//	DisconnectSession(pSession);
		//	LOG(L"SYSTEM", LOG_WARNNING, L"PacketBufferSize < PayloadSize ");
		//	break;
		//}
		pSession->RecvBuffer.RemoveData(sizeof(wHeader));

		// 4. PacketPool�� Packet ������ �Ҵ�
		if (pSession->RecvBuffer.Peek(pPacket->GetBufferPtr() + 2, wHeader) != wHeader)
		{
			pPacket->SubRef();
			LOG(L"SYSTEM", LOG_WARNNING, L"RecvQ dequeue error");
			DisconnectSession(pSession);
			break;
		}

		pPacket->MoveWritePos(wHeader);
		//UINT64 data;
		//memcpy(&data, pPacket->GetBufferPtr() + 2, 8);
		////wprintf(L"Recv data : %08ld\n", data);
		// 5. Packet ó��
		pSession->RecvBuffer.RemoveData(wHeader);
		InterlockedIncrement64(&_lRecvTPS);

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
		InterlockedIncrement64(&_lSendTPS);
	}
	pSession->dwPacketCount = 0;

	InterlockedExchange(&pSession->bIsSend, FALSE);
	PostSend(pSession);
}
