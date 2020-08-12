#include "stdafx.h"
#include "CNetWorkLibrary.h"
#include "CLog.h"

BOOL joshua::NetworkLibrary::InitialNetwork(const WCHAR* ip, DWORD port, BOOL Nagle)
{
	int retval;
	bool fail = false;

	SYSLOGCLASS* _pLog = SYSLOGCLASS::GetInstance();

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s\n", L"WSAStartup() Error!");
		return fail;
	}

	// socket
	_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_listen_socket == INVALID_SOCKET)
	{
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s\n", L"socket() Error!");
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
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s\n", L"bind() Error!");
		return fail;
	}

	//// 소켓 Send버퍼의 크기를 0으로 만들자
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
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s\n", L"listen() Error!");
		return fail;
	}
	return (!fail);
}

BOOL joshua::NetworkLibrary::CreateThread(DWORD threadCount)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	HANDLE hThread = NULL;
	//  Accept 스레드 생성
	_ThreadVector.push_back((HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)this, NULL, NULL));
	if (_ThreadVector[0] == NULL)
	{
		return FALSE;
	}
	CloseHandle(_ThreadVector[0]);
	// 스레드 제한
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
	for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	{
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
	SYSLOGCLASS* _pLog = SYSLOGCLASS::GetInstance();
	int temp = PopIndex();
	if (temp == -1)
	{
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"IndexStack Lack. CurrentSesionCount : %d\n", _dwSessionCount);
		return temp;
	}
	else
	{
		InterlockedIncrement64(&_dwSessionCount);
		InterlockedIncrement64(&_dwCount);
		_SessionArray[temp].SessionID = _dwSessionID++;
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
	srand(GetTickCount() + 1);
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
			_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s client IP : %s, port : %d\n", L"accept() Error!", szParam, ntohs(clientaddr.sin_port));
			closesocket(clientsocket);
			continue;
		}
		
		if (OnConnectionRequest(&clientaddr) == FALSE)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			_pLog->LOG(L"SERVER", LOG_LEVEL::e_WARNNING, L"client IP : %s, port : %d connection denied\n", szParam, ntohs(clientaddr.sin_port));
			closesocket(clientsocket);
			continue;
		}

		if (_dwSessionCount >= _dwSessionMax)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			_pLog->LOG(L"SERVER", LOG_LEVEL::e_WARNNING, L"client IP : %s, port : %d connection denied, MaxClient Over\n", szParam, ntohs(clientaddr.sin_port));
			closesocket(clientsocket);
			continue;
		}

		DWORD index = InsertSession(clientsocket, &clientaddr);
		if (index == -1)
		{
			// 서버 꺼야함
			// TODO dump내고 끄자
			closesocket(clientsocket);
			continue;
		}


		// 소켓과 입출력 완료 포트 연결
		CreateIoCompletionPort((HANDLE)clientsocket, (HANDLE)_hCP, (ULONG_PTR)&_SessionArray[index], 0);

		OnClientJoin(&_SessionArray[index].clientaddr, _SessionArray[index].SessionID);

	
		PostRecv(&_SessionArray[index]);
	}
	return;
}

void joshua::NetworkLibrary::WorkerThread(void)
{
	int retval;
	while (true)
	{
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred = 0;
		SOCKET client_sock = NULL;
		LPNEWWSAOVERLAPPED overlapped;
		st_SESSION* ptr = nullptr;
		ZeroMemory(&overlapped, sizeof(overlapped));

		retval = GetQueuedCompletionStatus(_hCP, &cbTransferred, (PULONG_PTR)&ptr, (LPOVERLAPPED*)&overlapped, INFINITE);

		if (ptr == nullptr && overlapped == NULL)
			break;

		if (ptr == nullptr && !overlapped && cbTransferred == NULL)
			break;

		if (retval == FALSE)
		{
			DisconnectSession(ptr->SessionID);
			goto HERE;
		}
		else
		{
			if (cbTransferred == 0)
			{
				DisconnectSession(ptr->SessionID);
				goto HERE;
			}

			if (overlapped->ID == READ)
			{
				ptr->RecvBuffer->MoveWritePos(cbTransferred);
				while (true)
				{
					int iRecvQSize = ptr->RecvBuffer->GetUseSize();
					WORD wHeaderLength;
					if (iRecvQSize < 10)
						break;

					// 패킷 검사
					ptr->RecvBuffer->Peek((char*)&wHeaderLength, sizeof(wHeaderLength));

					if (wHeaderLength != 8)
						break;

					if (wHeaderLength + sizeof(wHeaderLength) > ptr->RecvBuffer->GetUseSize())
						break;

					ptr->RecvBuffer->RemoveData(2);

					CMessage* newMessage = CMessage::Alloc();

					newMessage->AddRef();

					newMessage->PutData((char*)&wHeaderLength, sizeof(wHeaderLength));

					if (wHeaderLength != ptr->RecvBuffer->Peek(newMessage->GetBufferPtr() + 2, wHeaderLength))
						break;

					newMessage->MoveWritePos(wHeaderLength);

					ptr->RecvBuffer->RemoveData(wHeaderLength);

					OnRecv(ptr->SessionID, newMessage);
					newMessage->SubRef();
				}
				PostRecv(ptr);
			}
			else if (overlapped->ID == WRITE)
			{
				ZeroMemory(&ptr->SendOverlapped->Overlapped, sizeof(WSAOVERLAPPED));
				for (std::list<CMessage*>::iterator itor = ptr->lMessageList.begin(); itor != ptr->lMessageList.end();)
				{
					(*itor)->SubRef();
					itor = ptr->lMessageList.erase(itor);
				}
				ptr->dwPacketCount = 0;

				if (InterlockedExchange(&ptr->bIsSend, FALSE) == TRUE)
				{
					if (ptr->SendBuffer->GetUseSize() > 0)
						PostSend(ptr);
				}

			}
		}
HERE:
		if(InterlockedDecrement(&ptr->dwIOCount) <= 0)
		{
			SessionRelease(ptr->SessionID);
		}
	}
	return;
}

bool joshua::NetworkLibrary::PostSend(st_SESSION* session)
{
	int retval = 0;
	DWORD flags = 0;
	DWORD sendbytes;

	WSABUF wsaBuf[MAX_PACKET_COUNT];
	if (InterlockedExchange(&(session->bIsSend), TRUE) == FALSE)
	{
		int size = session->SendBuffer->GetUseSize();
		if (size == 0 || session->SessionID == -1)
		{
			InterlockedExchange(&session->bIsSend, FALSE);
			return true;
		}
		// 사용량으로 분기 놔눠서 보내자
		//if ((session->SendBuffer->GetUseSize() / 8) <= MAX_PACKET_COUNT)
		{
			int i = 0;
			while (size > 0)
			{
				if ( i >= MAX_PACKET_COUNT)
					break;
				CMessage* packet = nullptr;
				session->SendBuffer->Get((char*)&packet, 8);
				if (packet == nullptr)
					break;
				wsaBuf[i].buf = packet->GetBufferPtr();
				wsaBuf[i].len = packet->GetDataSize();
				InterlockedIncrement(&session->dwPacketCount);
				session->lMessageList.push_back(packet);
				i++;
				size -= 8;
			}
			InterlockedIncrement(&session->dwIOCount);
			ZeroMemory(&session->SendOverlapped->Overlapped, sizeof(WSAOVERLAPPED));
			retval = WSASend(session->socket, wsaBuf, session->dwPacketCount, &sendbytes, flags, &session->SendOverlapped->Overlapped, NULL);

		}

		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				if (WSAGetLastError() != WSAENOTSOCK && WSAGetLastError() != WSAECONNRESET)
				{
					wprintf(L"sessionID : %d, error : %d\n", session->SessionID, WSAGetLastError());
				}
				InterlockedExchange(&(session->bIsSend), FALSE);				
				if (InterlockedDecrement(&session->dwIOCount) <= 0)
				{
					DisconnectSession(session->SessionID);
					SessionRelease(session->SessionID);
				}
				return false;
			}
		}
	}
}

bool joshua::NetworkLibrary::PostRecv(st_SESSION* session)
{
	int retval;
	DWORD flags = 0;
	DWORD recvbytes;
	// 세션 초기화
	ZeroMemory(&session->RecvOverlapped->Overlapped, sizeof(WSAOVERLAPPED));

	//// 비동기 입출력 시작
	//if (session->SessionID == MAX_CLIENT_COUNT)
	//	return;
	if (session->RecvBuffer->GetFreeSize() <= 0 || session->SessionID == -1)
	{
		return true;
	}

	char* writebufptr = session->RecvBuffer->GetWriteBufferPtr();
	int dirputsize = session->RecvBuffer->GetNotBrokenPutSize();
	char* bufptr = session->RecvBuffer->GetBufferPtr();
	int freesize = session->RecvBuffer->GetFreeSize();
	// 같은데 쪼개서 보내야 할 경우
	if ((session->RecvBuffer->GetFreeSize() - session->RecvBuffer->GetNotBrokenPutSize()) > 0)
	{
		WSABUF wsaBuf[2];
		wsaBuf[0].buf = writebufptr;
		wsaBuf[0].len = dirputsize;
		wsaBuf[1].buf = bufptr;
		wsaBuf[1].len = freesize - dirputsize;
		//비동기 입출력 시작
		InterlockedIncrement(&session->dwIOCount);
		retval = WSARecv(session->socket, wsaBuf, 2, &recvbytes, &flags, &session->RecvOverlapped->Overlapped, NULL);

	}
	else if ((session->RecvBuffer->GetFreeSize() - session->RecvBuffer->GetNotBrokenPutSize()) == 0)
	{
		WSABUF wsaBuf;
		wsaBuf.buf = writebufptr;
		wsaBuf.len = dirputsize;
		InterlockedIncrement(&session->dwIOCount);
		retval = WSARecv(session->socket, &wsaBuf, 1, &recvbytes, &flags, &session->RecvOverlapped->Overlapped, NULL);
	}

	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			if (InterlockedDecrement(&session->dwIOCount) <= 0)
			{
				DisconnectSession(session->SessionID);
				SessionRelease(session->SessionID);
			}
			return false;
		}
	}
}


BOOL joshua::NetworkLibrary::Start(DWORD port, BOOL nagle, const WCHAR* ip, DWORD threadCount, DWORD MaxClient)
{
	SYSLOGCLASS* _pLog = SYSLOGCLASS::GetInstance();
	_pLog->SYSLOG_DIRECTORY(L"LogFile");
	_pLog->SYSLOG_LEVEL(LOG_LEVEL::e_DEBUG);
	// 소켓 초기화
	_dwSessionMax = MaxClient;

	if (InitialNetwork(ip, port, nagle) == false)
	{
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s\n", L"Initialize failed!");
		return FALSE;
	}

	// IOCP 생성
	_hCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_hCP == NULL)
	{
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s\n", L"CreateIoCompletionPort() ERROR");
		return FALSE;
	}

	// 스레드 생성
	if (CreateThread(threadCount) == FALSE)
	{
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s\n", L"CreateThread() ERROR");
		return FALSE;
	}

	// 세션 할당 및 인덱스 스택 설정
	if (CreateSession() == FALSE)
	{
		_pLog->LOG(L"SERVER", LOG_LEVEL::e_ERROR, L"%s\n", L"CreateSession() ERROR");
		return FALSE;
	}
	return TRUE;
}

void joshua::NetworkLibrary::SessionRelease(DWORD id)
{
	int index = 0;
	for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	{
		if (id == _SessionArray[i].SessionID)
		{
			index = i;
			break;
		}
	}
	_SessionArray[index].socket = INVALID_SOCKET;
	ZeroMemory(&_SessionArray[index].clientaddr, sizeof(SOCKADDR_IN));
	if (_SessionArray[index].SendBuffer->GetUseSize() > 0)
	{
		while (_SessionArray[index].SendBuffer->GetUseSize() > 0)
		{
			CMessage* packet;
			_SessionArray[index].SendBuffer->Get((char*)&packet, 8);
			_SessionArray[index].lMessageList.push_back(packet);
		}
	}
	for (std::list<CMessage*>::iterator itor = _SessionArray[index].lMessageList.begin(); itor != _SessionArray[index].lMessageList.end();)
	{
		(*itor)->SubRef();
		itor = _SessionArray[index].lMessageList.erase(itor);
	}
	_SessionArray[index].RecvBuffer->ClearBuffer();
	_SessionArray[index].SendBuffer->ClearBuffer();
	_SessionArray[index].lMessageList.clear();
	_SessionArray[index].dwPacketCount = 0;
	_SessionArray[index].SessionID = -1;
	_SessionArray[index].dwIOCount = 0;
	_SessionArray[index].bIsSend = FALSE;
	InterlockedDecrement64(&_dwSessionCount);
	PushIndex(index);
}

void joshua::NetworkLibrary::SendPacket(DWORD id, CMessage* message)
{
	int index = 0;
	for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	{
		if (id == _SessionArray[i].SessionID)
		{
			index = i;
			break;
		}
	}
	message->AddRef();
	_SessionArray[index].SendBuffer->Put((char*)&message, 8);
	PostSend(&_SessionArray[index]);
}

void joshua::NetworkLibrary::PrintPacketCount()
{
	wprintf(L"SessionCount : %d, Allock Count : %d\n", _dwSessionCount, CMessage::g_PacketPool->GetAllocCount());
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
