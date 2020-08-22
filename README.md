# Network-Library-IOCP-

## IOCP Library를 작성하여 간단한 Echo서버를 작성해보았다.

- 1단계. Session을 List 자료구조로 관리하였고 이에 동반되는 멀티 스레드 동기화 이슈를 해결하기 위해 동기화 객체를 사용하였다.

- 2단계. Session을 List로 관리하다보니 List에 접근 그리고 List의 삽입 삭제에 대한 동기화 이슈가 발생되고 이를 동기화 객체로 block하면서 해결하다 보니 성능이 좋지 않다.
        이에 Session을 Array의 형태로 관리해보자. 인덱스에 따라 세션이 삽입 삭제 되기 때문에 List에서 제기되던 참조 오류는 발생하지 않을 것.
        그리고 빈 공간의 인덱스를 stack자료구조로 관리하면 cache hit율도 높일 수 있을 것이다.
        대신 stack에 접근해서 push, pop하는 경우는 동기화 객체가 필요.
        
- 3단계. Send RingBuffer에 지금까지 패킷의 header와 payload를 담아서 이를 직접 send()했다면 지금 부터는 Send Ringbuffer에 Packet의 포인터를 담아 이를 꺼내 쓰는 용도로 사용하자.
        번거로워 보이나 이는 WSABUF 배열에 세팅을 해주고 한번에 보냄으로써 성능의 이점이 있다.
        또한 Packet을 MemoryPool에서 할당받아 재사용 함에 있어서 RefCounter 개념(smart pointer 내부 동작)을 도입해서 Packet 객체가 스스로 MemoryPool에 반납되는 구조를 만들자.
        MemoryPool에서 할당받아 이를 재활용 하는 과정은 매번 Packet객체를 생성 삭제 할 때보다 연산이 줄어들어 성능에 영향을 미칠 것이다.
        
        ------------------------------------------------------------- 현재까지의 작업(2020,08,22) ------------------------------------------------------------------
        
- 4단계. 현재 직접 제작한 RingBuffer를 사용하고 있으나 이것을 우리는 LockFree구조 형태로 변경 할 것이다.
        LockFree알고리즘은 SpinLock(원하고자 하는 자원 획득을 위해 지정된 시간동안 반복문을 돌면서 대기하는 것)과 비슷한데, 튼정 행동을 수행할 때까지 무한하게 반복 시도 하는 것이다.
        Lock알고리즘은 하나의 동기화 객체를 하나의 스레드가 점유하면 다른 스레드들은 block상태로 대기하나,
        LockFree알고리즘은 모든 스레드가 하나의 행동을 동시해 시도하면서 실행에 성공한 스레드는 하나씩 빠져나가는 구조.
        이것이 성능상에 이점이 있는지는 아직 논의가 필요하다.







## IOCP를 공부하면서 얻은 지식들

- 1. GQCS의 리턴과 매개 변수의 세팅: GQCS는 크게 TRUE or FALSE를 리턴한다.
                                  하지만 같은 FALSE라고 해도 매개변수의 값에 따라 그 의미와 상황이 다르기 때문에 적절한 대처가 필요하다.
                                  
     1. IOCP Queue로부터 완료패킷을 얻어내는 데 성공한 경우 => TRUE 리턴, lpCompletionKey 세팅

		 2. IOCP Queue로부터 완료패킷을 얻어내는 데 실패한 경우와 동시에 lpOverlapped가 NULL인 경우 => FALSE 리턴

		 3. IOCP Queue로부터 완료패킷을 얻어내는 데 성공한 경우와 동시에 
			  lpOverlapped가 NULL이 아닌 경우이면서 
		    dequeue한 요소가 실패한 I/O인 경우
		 => FALSE리턴, lpCompletionKey 세팅

		 4. IOCP에 등록된 소켓이 close된 경우
		 => FALSE 리턴, lpOverlapped에 NULL이 아니고,
		 => lpNumberOfBytes에 0 세팅.

		 5. 정상종료 (PQCS)
     		 => lpCompletionKey는 NULL, lpOverlapped는 NULL, lpNumberOfBytes또한 NULL
         
     1번의 경우는 정상적인 작동, 문제 없음.
     2번의 경우는 GQCS에러 에러코드 확인하고 지금은 서버를 끄는 쪽으로 해결하자.
     3번과 4번의 경우는 shutdown으로 세션의 연결을 끊자.
     5번의 경우는 정상적으로 서버를 끄고자 하는 경우

- 2. Shutdown()에 대해서.

	    shutdown() 함수는 4HandShake(1Fin - 2Ack+3Fin - 4Fin)에서 1Fin을 보내는 함수
	   1. 상대편에서 이에 대한 Ack를 보내지 않으면(대게 상대방이 '응답없음'으로 먹통됐을 경우) 종료가 되지 않음
	     - 이 현상(연결 끊어야하는데 안끊어지는 경우)이 지속되면 메모리가 순간적으로 증가하고 SendBuffer가 터질 수 있음
	   2. TimeWait이 남음
	     - CancelIoEx 사용하여 4HandShake를 무시하고 닫아야 남지 않는다
       
- 3. CancelIoEX()에 대해서
	    shutdown으로는 닫을 수 없을 경우 사용 (SendBuffer가 가득 찼을때, Heartbeat가 끊어졌을때)
