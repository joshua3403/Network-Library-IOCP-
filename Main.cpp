#include "stdafx.h"
#include "CNetworkTest.h"

int main()
{
	CMessage::SetMemoryPool(6000, FALSE);
	Network* test = new Network();

	test->Start(6000,1,NULL,NULL,200);

	while (true)
	{
		test->PrintPacketCount();
		Sleep(1000);
	}

	delete test;

	return 0;
}