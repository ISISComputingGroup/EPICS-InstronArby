/// @file drvAsynVISAPort.h ASYN driver for National Instruments VISA 

#ifndef DRVASYNINSTRONARBY_H
#define DRVASYNINSTRONARBY_H

#include <shareLib.h>  

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

epicsShareFunc int drvAsynInstronArbyConfigure(const char *portName,
                         int iDevice, 
                         unsigned int priority,
                         int noAutoConnect,
                         int noProcessEos);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* DRVASYNINSTRONARBY_H */
