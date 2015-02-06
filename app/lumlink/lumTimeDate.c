/*
******************************
*Company:Lumlink
*Data:2015-02-04
*Author:Meiyusong
******************************
*/


#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "espconn.h"


#include "lumlink/lumCommon.h"
#include "lumlink/lumTimeDate.h"


static TIME_DATE_INFO g_timeDateInfo;
static os_timer_t g_systemTimeTimer;
static os_timer_t g_networkTimeTimer;
static os_timer_t g_getTimeTimer;

static BOOL g_getUTCSucc;

static struct espconn timeConnHandle;
static struct _esp_tcp timeTcpHandle;


static void USER_FUNC lum_getNetworkTime(void);
static void USER_FUNC lum_syncNetworkTimer(U32 timeGap);



static void USER_FUNC lum_syncNetworkTime(U32 networkTime)
{
	g_timeDateInfo.lastUTCTime = networkTime - DIFF_SEC_1900_1970;
	g_timeDateInfo.lastSystemTime = system_get_time();
	lumDebug("============>network Time=%d\n", g_timeDateInfo.lastUTCTime);

}


static void USER_FUNC lum_timeSocketProtect(ETSTimerFunc pfunction)
{
	os_timer_disarm(&g_getTimeTimer);
	os_timer_setfn(&g_getTimeTimer, pfunction, NULL);
	os_timer_arm(&g_getTimeTimer, TIME_RECONNECT_TIMER_GAP, 0);
}



// set timeout while connect but not get time
static void USER_FUNC lum_disconnectTimerCallback(void *arg)
{
	espconn_disconnect(&timeConnHandle);
	if(!g_getUTCSucc)
	{
		lum_syncNetworkTimer(SYNC_NETWORK_TIME_TIMER_FAILD_GAP); //获取时间失败，减短等待时间
	}
}



static void USER_FUNC lum_timeRecvCallback(void *arg, char *pusrdata, unsigned short length)
{
	U32* tmp;
	U32 curUTC;


	tmp = (U32*)pusrdata;
	curUTC = ntohl(tmp[0]);
	lum_syncNetworkTime(curUTC);
	g_getUTCSucc = TRUE;
}


static void USER_FUNC lum_timeSentCallback(void *arg)
{
	//struct espconn *pespconn = arg;

	lumDebug("lum_tcpSentCallback\n");
}


static void USER_FUNC lum_timeConnectCallback(void *arg)
{
	struct espconn *pespconn = arg;


	lumDebug("lum_timeConnectCallback\n");
	espconn_regist_recvcb(pespconn, lum_timeRecvCallback);
	espconn_regist_sentcb(pespconn, lum_timeSentCallback);

	lum_timeSocketProtect(lum_disconnectTimerCallback);//链接后10秒，如没有断开则强制断开
}


static void USER_FUNC lum_reconnectTimerCallback(void *arg)
{
	lum_getNetworkTime();
}


static void USER_FUNC lum_timeDisconnectCallback(void *arg)
{
	struct espconn *pespconn = arg;


	lumDebug("lum_timeDisconnectCallback \n");
	os_timer_disarm(&g_getTimeTimer); //如已经断开，取消保护
	espconn_delete(pespconn);
}


static void USER_FUNC lum_timeReconnectCallback(void *arg, sint8 err)
{
	struct espconn *pespconn = (struct espconn *)arg;


	lumDebug("lum_timeReconnectCallback err=%d\n", err);
	if(g_getUTCSucc)
	{
		lum_timeDisconnectCallback(pespconn);
	}
	else
	{
		lum_timeSocketProtect(lum_reconnectTimerCallback);
	}
}


static void USER_FUNC lum_getNetworkTime(void)
{
	U32 ipAddr;

	lumDebug("lum_getNetworkTime\n");
	os_memset(&timeConnHandle, 0, sizeof(struct espconn));
	os_memset(&timeTcpHandle, 0, sizeof(struct _esp_tcp));

	timeConnHandle.proto.tcp = &timeTcpHandle;
	timeConnHandle.type = ESPCONN_TCP;
	timeConnHandle.state = ESPCONN_NONE;

	timeConnHandle.proto.tcp->remote_port = TCP_DATE_PORT;
	timeConnHandle.proto.tcp->local_port = espconn_port();
	ipAddr = ipaddr_addr(TCP_DATE_IP);
	os_memcpy(timeConnHandle.proto.tcp->remote_ip, &ipAddr, SOCKET_IP_LEN);

	espconn_regist_connectcb(&timeConnHandle, lum_timeConnectCallback);
	espconn_regist_reconcb(&timeConnHandle, lum_timeReconnectCallback);
	espconn_regist_disconcb(&timeConnHandle, lum_timeDisconnectCallback);

	espconn_connect(&timeConnHandle);
	g_getUTCSucc = FALSE;
}


static void USER_FUNC lum_syncSystemTime(void)
{
	U32 curTimeUs;
	U32 curTimeSecond;


	curTimeUs = system_get_time();
	if(curTimeUs < g_timeDateInfo.lastSystemTime)
	{
		curTimeUs = 0xFFFFFFFF - g_timeDateInfo.lastSystemTime + curTimeUs;
	}
	else
	{
		curTimeUs = curTimeUs - g_timeDateInfo.lastSystemTime;
	}

	curTimeSecond = curTimeUs/1000000;  //us-->S
	g_timeDateInfo.lastUTCTime += curTimeSecond;
	g_timeDateInfo.lastSystemTime = curTimeUs;
}


static void USER_FUNC lum_syncSystemTimerCallback(void *arg)
{
	lum_syncSystemTime();
}


static void USER_FUNC lum_syncSystemTimer(void)
{
	os_timer_disarm(&g_systemTimeTimer);
	os_timer_setfn(&g_systemTimeTimer, (os_timer_func_t *)lum_syncSystemTimerCallback, NULL);
	os_timer_arm(&g_systemTimeTimer, SYNC_SYSTEM_TIME_TIMER_GAP, 1);
}


static void USER_FUNC lum_syncNetworkTimerCallback(void *arg)
{
	lum_syncNetworkTimer(SYNC_NETWORK_TIME_TIMER_SUCC_GAP);
	lum_getNetworkTime();
}


static void USER_FUNC lum_syncNetworkTimer(U32 timeGap)
{
	os_timer_disarm(&g_networkTimeTimer);
	os_timer_setfn(&g_networkTimeTimer, (os_timer_func_t *)lum_syncNetworkTimerCallback, NULL);
	os_timer_arm(&g_networkTimeTimer, timeGap, 0);
}


//init system time immediatly after power
void USER_FUNC lum_initSystemTime(void)
{
	g_timeDateInfo.lastSystemTime = 0;
	g_timeDateInfo.lastUTCTime = SEC_2015_01_01_00_00_00;
	lum_syncSystemTimer();
}


//init network time after DHPC
void USER_FUNC lum_initNetworkTime(void)
{
	lum_syncNetworkTimer(10000);
}


static U32 USER_FUNC lum_getSystemTime(void)
{
	U32 curTimeUs;
	U32 curTimeSecond;


	curTimeUs = system_get_time();
	if(curTimeUs < g_timeDateInfo.lastSystemTime)
	{
		curTimeUs = 0xFFFFFFFF - g_timeDateInfo.lastSystemTime + curTimeUs;
	}
	else
	{
		curTimeUs = curTimeUs - g_timeDateInfo.lastSystemTime;
	}
	curTimeSecond = curTimeUs/1000000;  //us-->S
	return g_timeDateInfo.lastUTCTime + curTimeSecond;
}




const U32 SEC_PER_YR[2] = { 31536000, 31622400 };
const U32 SEC_PER_MT[2][12] =
{
	{ 2678400, 2419200, 2678400, 2592000, 2678400, 2592000, 2678400, 2678400, 2592000, 2678400, 2592000, 2678400 },
	{ 2678400, 2505600, 2678400, 2592000, 2678400, 2592000, 2678400, 2678400, 2592000, 2678400, 2592000, 2678400 },
};
const U8 WEEK_DAY_INFO[] =  {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
#define SEC_PER_DY		86400
#define SEC_PER_HR		3600



static inline U8 lum_bLeapYear(U16 yearData)
{
	if (!(yearData%100))
	{
		return (yearData%400 == 0) ? 1 : 0;
	}
	else
	{
		return (yearData%4 == 0) ? 1 : 0;
	}
}


static U8 USER_FUNC lum_getDayOfWeek(U8 month, U8 day, U16 year)
{
	/* Month should be a number 0 to 11, Day should be a number 1 to 31 */

	year -= month < 3;
	return (year + year/4 - year/100 + year/400 + WEEK_DAY_INFO[month-1] + day) % 7;
}


static void USER_FUNC lum_gmtime(U32 second, TIME_DATA_INFO* timeInfo)
{
	U32 tmpSecond;
	U8 bLeepYear;


	tmpSecond = second;
	os_memset(timeInfo, 0, sizeof(TIME_DATA_INFO));

	timeInfo->year = 1970;
	while(1)
	{
		if(tmpSecond < SEC_PER_YR[lum_bLeapYear(timeInfo->year)])
		{
			break;
		}
		tmpSecond -= SEC_PER_YR[lum_bLeapYear(timeInfo->year)];
		timeInfo->year++;
	}

	bLeepYear = lum_bLeapYear(timeInfo->year);

	while(1)
	{
		if(tmpSecond < SEC_PER_MT[bLeepYear][timeInfo->month])
		{
			break;
		}
		tmpSecond -= SEC_PER_MT[bLeepYear][timeInfo->month];
		timeInfo->month++;
	}

	timeInfo->day = tmpSecond / SEC_PER_DY;
	timeInfo->day++;
	tmpSecond = tmpSecond % SEC_PER_DY;

	timeInfo->hour = tmpSecond / SEC_PER_HR;
	tmpSecond = tmpSecond % SEC_PER_HR;

	timeInfo->minute = tmpSecond / 60;
	timeInfo->second = tmpSecond % 60;

	timeInfo->week = lum_getDayOfWeek((timeInfo->month + 1), timeInfo->day, timeInfo->year);
}



void USER_FUNC lum_getGmtime(TIME_DATA_INFO* timeInfo)
{
	U32 curSecond;


	curSecond = lum_getSystemTime();
	lum_gmtime(curSecond, timeInfo);
}


void USER_FUNC lum_getStringTime(S8* timeData)
{
	U32 curTime;
	TIME_DATA_INFO timeInfo;


	curTime = lum_getSystemTime();
	lum_gmtime(curTime, &timeInfo);

	os_sprintf(timeData, "%04d-%02d-%02d %02d:%02d:%02d <%d>", timeInfo.year, timeInfo.month+1,
		timeInfo.day, timeInfo.hour, timeInfo.minute, timeInfo.second, timeInfo.week);
}

