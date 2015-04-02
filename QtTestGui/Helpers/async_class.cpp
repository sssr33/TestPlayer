#if INCLUDE_PCH_H == 1
#include "pch.h"
#endif
#include "async_class.h"

async_class::async_class()
	: shuttingDown(false), asyncOpCount(0){
}