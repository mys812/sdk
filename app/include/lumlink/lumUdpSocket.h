/*
******************************
*Company:Lumlink
*Data:2015-01-26
*Author:Meiyusong
******************************
*/

#ifndef __LUMLINK_UDP_SOCKET_H__
#define __LUMLINK_UDP_SOCKET_H__


void lum_udpSocketInit(void);
void lum_sendUdpData(U8* socketData, U8 dataLen, U32 ipData);


#endif
