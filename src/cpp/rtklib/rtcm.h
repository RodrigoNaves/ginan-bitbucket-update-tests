// /*
// #ifndef __RTCM_H
// #define __RTCM_H
//
// #include "gaTime.hpp"
// #include "observations.hpp"
// #include "navigation.hpp"
// #include "station.hpp"
// #include "constants.h"
//
// struct rtcm_t
// {        /* RTCM control struct type */
//     int staid;          /* station id */
//     int stah;           /* station health */
//     int seqno;          /* sequence number for rtcm 2 or iods msm */
//     int outtype;        /* output message type */
//     gtime_t time;       /* message time */
//     gtime_t time_s;     /* message start time */
//     obs_t obs;          /* observation data (uncorrected) */
//     nav_t nav;          /* satellite ephemerides */
//     sta_t sta;          /* station parameters */
//     ssr_t ssr[MAXSAT];  /* output of ssr corrections */
//     char msg[128];      /* special message */
//     char msgtype[256];  /* last message type */
//     char msmtype[6][128]; /* msm signal types */
//     int obsflag;        /* obs data complete flag (1:ok,0:not complete) */
//     int ephsat;         /* update satellite of ephemeris */
//     double cp[MAXSAT][NFREQ+NEXOBS]; /* carrier-phase measurement */	//todo aaron, maps
//     unsigned char lock[MAXSAT][NFREQ+NEXOBS]; /* lock time */
//     unsigned char loss[MAXSAT][NFREQ+NEXOBS]; /* loss of lock count */
//     gtime_t lltime[MAXSAT][NFREQ+NEXOBS]; /* last lock time */
//     int nbyte;          /* number of bytes in message buffer */
//     int nbit;           /* number of bits in word buffer */
//     int len;            /* message length (bytes) */
//     unsigned char buff[1200]; /* message buffer */
//     unsigned int word;  /* word buffer for rtcm 2 */
//     unsigned int nmsg2[100]; /* message count of RTCM 2 (1-99:1-99,0:other) */
//     unsigned int nmsg3[400]; /* message count of RTCM 3 (1-299:1001-1299,300-399:2000-2099,0:ohter) */
//     char opt[256];      /* RTCM dependent options */
// };
//
//
// #endif*/
