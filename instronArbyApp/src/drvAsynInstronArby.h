/// @file drvAsynInstronArby.h ASYN driver for Instron Arby interface 

#ifndef DRVASYNINSTRONARBY_H
#define DRVASYNINSTRONARBY_H

#include <shareLib.h>  

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

epicsShareFunc int drvAsynInstronArbyConfigure(const char *portName,
                         int iDevice,
                         const char* fakeReadTerminator,
                         unsigned int priority,
                         int noAutoConnect,
                         int noProcessEos);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* DRVASYNINSTRONARBY_H */
