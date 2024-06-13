// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2022 by "SteelT".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  http-serv.c
/// \brief HTTP server

#ifdef HAVE_HTTPSERV
#ifndef __HTTP_SERV_H__
#define __HTTP_SERV_H__

#define HTTP_SV_PORT 8888

void HTTPSV_StartServer(void);
void HTTPSV_StopServer(void);

#endif // __HTTP_SERV_H__
#endif // HAVE_HTTPSERV