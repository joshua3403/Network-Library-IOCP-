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
		// TCP 소켓에서 좀비세션 처리
		// 좀비 세션은 비정상적으로 종료(랜선 문제, 혹은 외부에서 세션을 일부러 끊은 경우)되어 서버나 클라이언트에서
		// 세션이 종료된지 모르는 상태의 세션을 의미 (Close 이벤트를 받지 못함)
		// TCP KeepAlive사용
				// - SO_KEEPALIVE : 시스템 레지스트리 값 변경. 시스템의 모든 SOCKET에 대해서 KEEPALIVE 설정
		// - SIO_KEEPALIVE_VALS : 특정 SOCKET만 KEEPALIVE 설정
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
			// 서버 꺼야함
			// TODO dump내고 끄자
			closesocket(clientsocket);
			continue;
		}


		// 소켓과 입출력 완료 포트 연결
		if (CreateIoCompletionPort((HANDLE)clientsocket, (HANDLE)_hCP, (ULONG_PTR)&_SessionArray[index], 0) == NULL)
		{
			//  HANDLE 인자에 소켓이 아닌 값이 올 경우 잘못된 핸들(6번 에러) 발생
			// 소켓이 아닌 값을 넣었다는 것은 다른 스레드에서 소켓을 반환했다는 의미이므로 동기화 문제일 가능성이 높다.
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
	// 비동기 입출력 완료 기다리기
	DWORD cbTransferred;
	LPOVERLAPPED pOverlapped;
	st_SESSION* pSession = nullptr;
	while (true)
	{
		cbTransferred = 0;
		pOverlapped = 0;
		pSession = nullptr;

		// GQCS return 경우의 수.
		// 1. IOCP Queue로부터 완료패킷을 얻어내는 데 성공한 경우 => TRUE 리턴, lpCompletionKey 세팅

		// 2. IOCP Queue로부터 완료패킷을 얻어내는 데 실패한 경우와 동시에 lpOverlapped가 NULL인 경우 => FALSE 리턴

		// 3. IOCP Queue로부터 완료패킷을 얻어내는 데 성공한 경우와 동시에 
		//	  lpOverlapped가 NULL이 아닌 경우이면서 
		//    dequeue한 요소가 실패한 I/O인 경우
		// => FALSE리턴, lpCompletionKey 세팅

		// 4. IOCP에 등록된 소켓이 close된 경우
		// => FALSE 리턴, lpOverlapped에 NULL이 아니고,
		// => lpNumberOfBytes에 0 세팅.

		// 5. 정상종료 (PQCS)
		// => lpCompletionKey는 NULL, lpOverlapped는 NULL, lpNumberOfBytes또한 NULL

		retval = GetQueuedCompletionStatus(_hCP, &cbTransferred, reinterpret_cast<PULONG_PTR>(&pSession), (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (pOverlapped == NULL)
		{
			// 2번.
			if (retval == FALSE)
			{
				LOG(L"SYSTEM", LOG_ERROR, L"GetQueuedCompletionStatus() failed %d", GetLastError());
				PostQueuedCompletionStatus(_hCP, 0, 0, 0);	// GQCS 에러 시 조치를 취할만한게 없으므로 종료
				break;
			}

			// 정상종료
			if (pSession == NULL && cbTransferred == NULL)
			{
				PostQueuedCompletionStatus(_hCP, 0, 0, 0);	// GQCS 에러 시 조치를 취할만한게 없으므로 종료
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
		// 더 보낼 데이터가 없는 경우
		if (session->SendBuffer.GetUseSize() == 0)
			return false;
		// send flag가 변경이 안된 경우
		if (InterlockedCompareExchange(&session->bIsSend, TRUE, FALSE) == TRUE)
			return false;

		// 보낼 데이터가 있었으나 다른 스레드에서 완료통지로
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

		// WSASend가 pending이 걸릴 경우는 현재 send 임시 버퍼가 가득찬 상태
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

		// WSASend가 pending이 걸릴 경우는 현재 send 임시 버퍼가 가득찬 상태
		if (error != WSA_IO_PENDING)
		{			// WSAENOTSOCK(10038) : 소켓이 아닌 항목에 소켓 작업 시도
			// WSAECONNABORTED(10053) : 호스트가 연결 중지. 데이터 전송 시간 초과, 프로토콜 오류 발생
			// WSAECONNRESET(10054) : 원격 호스트에 의해 기존 연결 강제 해제. 원격 호스트가 갑자기 중지되거나 다시 시작되거나 하드 종료를 사용하는 경우
			// WSAESHUTDOWN(10058) : 소켓 종료 후 전송
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
	// 소켓 초기화
	_dwSessionMax = MaxClient;

	if (InitialNetwork(ip, port, nagle) == false)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"Initialize failed!");
		return FALSE;
	}

	// IOCP 생성
	_hCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_hCP == NULL)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"CreateIoCompletionPort() ERROR");
		return FALSE;
	}

	// 스레드 생성
	if (CreateThread(threadCount) == FALSE)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"CreateThread() ERROR");
		return FALSE;
	}

	// 세션 할당 및 인덱스 스택 설정
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
	// multi-thread 환경에서 release, connectm send, accept 등이 동시에 발생할 수 있음을 생각해야 한다.
	st_SESSION* pSession = nullptr;

	for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	{
		if (_SessionArray[i].SessionID == iSessionID)
		{
			pSession = &_SessionArray[i];
			break;
		}
	}

	// 1. ReleaseFlag가 확실하게 변경된 상태
	if (pSession->bIsReleased == TRUE)
		return nullptr;

	// 2. ReleaseSession 함수 내에 있는 경우
	// 이때 다른 스레드에서 SendPacket이나 Disconnect 시도 시, IOCOUNT가 1이 될 수 있음
	if (InterlockedIncrement(&pSession->dwIOCount) == 1)
	{
		if (InterlockedDecrement(&pSession->dwIOCount) == 0)
			SessionRelease(pSession);

		return nullptr;
	}

	// 3. 이미 disconnect 된 다음 accept하여 새 session으로 대체된 경우
	if (pSession->SessionID != iSessionID)
	{
		if(InterlockedDecrement(&pSession->dwIOCount) == 0)
			SessionRelease(pSession);
		return nullptr;
	}

	// 4. releaseFlag가 FALSE인 경우
	if (pSession->bIsReleased == FALSE)
		return pSession;

	// 5. 4번 진입하기 직전에 TRUE가 된 경우
	if (InterlockedIncrement(&pSession->dwIOCount) == 0)
		SessionRelease(pSession);
	return nullptr;
}

void joshua::NetworkLibrary::RecvComplete(st_SESSION* pSession, DWORD dwTransferred)
{	// 수신량만큼 RecvQ.MoveWritePos 이동
	pSession->RecvBuffer.MoveWritePos(dwTransferred);

	// Header는 Payload 길이를 담은 2Byte 크기의 타입
	WORD wHeader;
	while (1)
	{
		// 1. RecvQ에 'sizeof(Header)'만큼 있는지 확인
		int iRecvSize = pSession->RecvBuffer.GetUseSize();
		if (iRecvSize < sizeof(wHeader))
		{
			// 더 이상 처리할 패킷이 없음
			// 만약, Header 크기보다 작은 모종의 데이터가 누적된다면 2번에서 걸러짐
			break;
		}

		// 2. Packet 길이 확인 : Header크기('sizeof(Header)') + Payload길이('Header')
		pSession->RecvBuffer.Peek(reinterpret_cast<char*>(&wHeader), sizeof(wHeader));
		if (iRecvSize < sizeof(wHeader) + wHeader)
		{
			// Header의 Payload의 길이가 실제와 다름
			shutdown(pSession->socket, SD_BOTH);
			LOG(L"SYSTEM", LOG_WARNNING, L"Header & PayloadLength mismatch");
			break;
		}
		// RecvQ에서 Packet의 Header 부분 제거
		pSession->RecvBuffer.MoveWritePos(sizeof(wHeader));

		CMessage* pPacket = CMessage::Alloc();

		// 3. Payload 길이 확인 : PacketBuffer의 최대 크기보다 Payload가 클 경우
		if (pPacket->GetBufferSize() < wHeader)
		{
			pPacket->SubRef();
			shutdown(pSession->socket, SD_BOTH);
			LOG(L"SYSTEM", LOG_WARNNING, L"PacketBufferSize < PayloadSize ");
			break;
		}

		// 4. PacketPool에 Packet 포인터 할당
		if (pSession->RecvBuffer.Get(pPacket->GetBufferPtr(), wHeader) != wHeader)
		{
			pPacket->SubRef();
			LOG(L"SYSTEM", LOG_WARNNING, L"RecvQ dequeue error");
			shutdown(pSession->socket, SD_BOTH);
			break;
		}

		// RecvQ에서 Packet의 Payload 부분 제거
		pPacket->MoveWritePos(wHeader);

		// 5. Packet 처리
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
