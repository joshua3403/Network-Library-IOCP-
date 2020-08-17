#include "stdafx.h"
#include "CNetworkTest.h"

int main()
{
	CMessage::SetMemoryPool(0, FALSE);
	Network* test = new Network();

	test->Start(6000,TRUE,NULL,NULL,500);

	while (true)
	{
		test->PrintPacketCount();
		Sleep(1000);
	}

	delete test;

	return 0;
}