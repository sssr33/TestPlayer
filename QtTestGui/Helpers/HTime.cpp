#if INCLUDE_PCH_H == 1
#include "pch.h"
#endif
#include "HTime.h"

#include <chrono>
#include <ctime>
#include <cstdint>

uint64_t HTime::GetTimeMsRelative(){
#if defined(HAVE_VISUAL_STUDIO)
	return GetTickCount64();
#else
#error Add impl
#endif
}

uint64_t HTime::UnixTimeToLib(uint64_t unixTime){
	uint64_t libTime = HTime::Y1970Ticks + (unixTime * HTime::HNSResolution);
	return libTime;
}

uint64_t HTime::LibTimeToUnix(uint64_t libTime){
	uint64_t unixTime = (libTime - HTime::Y1970Ticks) / HTime::HNSResolution;
	return unixTime;
}

uint64_t HTime::GetCurrentLibTime(){
	typedef std::ratio<1, static_cast<int64_t>(HTime::HNSResolution)> hns;
	typedef std::chrono::duration<int64_t, hns> hundred_nanoseconds;
	uint64_t libTime = 0;
	auto nowTp = std::chrono::system_clock::now();
	uint64_t nowSysHns = static_cast<uint64_t>(std::chrono::duration_cast<hundred_nanoseconds>(nowTp.time_since_epoch()).count());

	libTime = HTime::GetSysClockEpochTicks() + nowSysHns;

	return libTime;
}

uint64_t HTime::CeilLibTimeToSeconds(uint64_t libTime){
	uint64_t millis = libTime % HTime::HNSResolution;

	libTime -= millis;

	if (millis != 0){
		libTime += HTime::HNSResolution;
	}

	return libTime;
}

#ifdef HAVE_VISUAL_STUDIO
uint64_t HTime::FileTimeToLib(FILETIME fileTime){
	uint64_t tmpFileTime = static_cast<uint64_t>(fileTime.dwLowDateTime) | (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32);
	uint64_t libTime = HTime::Y1601Ticks + tmpFileTime;
	return libTime;
}

FILETIME HTime::LibTimeToFile(uint64_t libTime){
	uint64_t tmpFileTime = libTime - HTime::Y1601Ticks;
	FILETIME fileTime;

	fileTime.dwLowDateTime = static_cast<DWORD>(tmpFileTime & 0x00000000FFFFFFFF);
	fileTime.dwHighDateTime = static_cast<DWORD>((tmpFileTime >> 32) & 0x00000000FFFFFFFF);

	return fileTime;
}

uint64_t HTime::UniversalTimeToLib(int64_t universalTime){
	uint64_t libTime = static_cast<uint64_t>(static_cast<int64_t>(HTime::Y1601Ticks) + universalTime);
	return libTime;
}

int64_t HTime::LibTimeToUniversal(uint64_t libTime){
	int64_t universalTime = static_cast<int64_t>(libTime - HTime::Y1601Ticks);
	return universalTime;
}

uint64_t HTime::LibTimeToLocal(uint64_t libTime){
	uint64_t localLibTime;
	SYSTEMTIME st, st_utc;
	auto ft = HTime::LibTimeToFile(libTime);

	FileTimeToSystemTime(&ft, &st_utc);
	SystemTimeToTzSpecificLocalTime(nullptr, &st_utc, &st);
	SystemTimeToFileTime(&st, &ft);

	localLibTime = HTime::FileTimeToLib(ft);

	return localLibTime;
}

uint64_t HTime::LibTimeToGlobal(uint64_t localLibTime){
	uint64_t libTime;
	SYSTEMTIME st, st_utc;
	auto ft = HTime::LibTimeToFile(localLibTime);

	FileTimeToSystemTime(&ft, &st);
	TzSpecificLocalTimeToSystemTime(nullptr, &st, &st_utc);
	SystemTimeToFileTime(&st_utc, &ft);

	libTime = HTime::FileTimeToLib(ft);

	return libTime;
}
#endif

uint64_t HTime::GetSysClockEpochTicks(){
	static uint64_t SysClockEpochTicks = 0;

	if (SysClockEpochTicks == 0){
		std::chrono::system_clock::time_point epochTp;
		tm epochTm;
		auto epochTimeT = std::chrono::system_clock::to_time_t(epochTp);
		gmtime_s(&epochTm, &epochTimeT);
		uint64_t epochFullTicks = HTime::YearTicks * (epochTm.tm_year + 1900);
		uint64_t epochLeapTicks = epochFullTicks % HTime::DayTicks;
		epochFullTicks -= epochLeapTicks;

		epochFullTicks += HTime::DayTicks * epochTm.tm_yday;
		epochFullTicks += HTime::HourTicks * epochTm.tm_hour;
		epochFullTicks += HTime::MinuteTicks * epochTm.tm_min;
		epochFullTicks += HTime::HNSResolution * epochTm.tm_sec;

		SysClockEpochTicks = epochFullTicks;
	}

	return SysClockEpochTicks;
}