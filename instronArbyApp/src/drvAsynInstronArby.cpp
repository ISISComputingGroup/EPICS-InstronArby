/// @file drvAsynInstronArby.cpp ASYN driver for National Instruments VISA 

#include <windows.h>

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <osiUnistd.h>
#include <cantProceed.h>
#include <errlog.h>
#include <iocsh.h>
#include <epicsAssert.h>
#include <epicsExit.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <osiUnistd.h>

#include <iostream>
#include <string>


#include "asynDriver.h"
#include "asynOctet.h"
#include "asynOption.h"
#include "asynInterposeCom.h"
#include "asynInterposeEos.h"

#include <epicsExport.h>

#include "drvAsynInstronArby.h"

/// driver private data structure
typedef struct {
    asynUser          *pasynUser; 
    char              *portName;  ///< asyn port name
	bool               connected;  ///< are we currently connected 
    int                deviceId; ///< VISA resource name session connected to 
    unsigned long      nReadBytes;  ///< number of bytes read from this resource name
    unsigned long      nWriteBytes; ///< number of bytes written to this resource
    unsigned long      nReadCalls;  ///< number of read calls from this resource name
    unsigned long      nWriteCalls; ///< number of written calls to this resource
    asynInterface      common;
    asynInterface      octet;
    std::string        replyData;
    std::string        fakeReadTerminator;
} instronDriver_t;


/// close a VISA session
static asynStatus
closeConnection(asynUser *pasynUser, instronDriver_t *driver, const char* reason)
{
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Close %s connection %s\n", driver->portName, reason);
    if (!driver->connected) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: session already closed", driver->portName);
        return asynError;
    }
    driver->connected = false;
	pasynManager->exceptionDisconnect(pasynUser);
    return asynSuccess;
}


/// asynCommon interface - Report link parameters
static void
asynCommonReport(void *drvPvt, FILE *fp, int details)
{
    instronDriver_t *driver = (instronDriver_t*)drvPvt;
    assert(driver);
    if (details >= 1) {
        fprintf(fp, "    deviceId %d: %sonnected\n",
                                                driver->deviceId,
                                                (driver->connected ? "C" : "Disc"));
        char termChar[16];
        epicsStrnEscapedFromRaw(termChar, sizeof(termChar), driver->fakeReadTerminator.c_str(), driver->fakeReadTerminator.size());
        fprintf(fp, "    fakeReadTerminator: %s\n", termChar);
    }
    if (details >= 2) {
        fprintf(fp, "    Characters written: %lu\n", driver->nWriteBytes);
        fprintf(fp, "       Characters read: %lu\n", driver->nReadBytes);
        fprintf(fp, "      write operations: %lu\n", driver->nWriteCalls);
        fprintf(fp, "       read operations: %lu\n", driver->nReadCalls);
    }
}

static void
instronCleanup (void *arg)
{
    asynStatus status;
    instronDriver_t *driver = (instronDriver_t*)arg;
	
    if (!arg) return;
    status=pasynManager->lockPort(driver->pasynUser);
    if(status!=asynSuccess)
        asynPrint(driver->pasynUser, ASYN_TRACE_ERROR, "%s: cleanup locking error\n", driver->portName);

    if(status==asynSuccess)
        pasynManager->unlockPort(driver->pasynUser);

}

static void
driverCleanup(instronDriver_t *driver)
{
	if (driver)
	{
        free(driver->portName);
        free(driver);
    }
}

/// create a link
static asynStatus
connectIt(void *drvPvt, asynUser *pasynUser)
{
    instronDriver_t *driver = (instronDriver_t*)drvPvt;
    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Open connection to \"%d\"  reason: %d\n", driver->deviceId,
                                                           pasynUser->reason);

    if (driver->connected) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: session already open.", driver->portName);
        return asynError;
    }
    driver->connected = true;
    return asynSuccess;
}


static asynStatus
asynCommonConnect(void *drvPvt, asynUser *pasynUser)
{
    asynStatus status = asynSuccess;

    status = connectIt(drvPvt, pasynUser);
    if (status == asynSuccess)
        pasynManager->exceptionConnect(pasynUser);
    return status;
}

static asynStatus
asynCommonDisconnect(void *drvPvt, asynUser *pasynUser)
{
    instronDriver_t *driver = (instronDriver_t*)drvPvt;

    assert(driver);
    return closeConnection(pasynUser,driver,"Disconnect request");
}


// BOOL WINAPI Arby_SendString (int iDevice, PSTR sCommand)
typedef BOOL (__stdcall * ArbySendString_t)(int, PSTR);

// BOOL WINAPI Arby_QueryString (int iDevice, PSTR sQuery, PSTR sReply, long lRLen);
typedef BOOL (__stdcall * ArbyQueryString_t)(int, PSTR, PSTR, long);

/// write values to device
static asynStatus writeIt(void *drvPvt, asynUser *pasynUser,
    const char *data, size_t numchars, size_t *nbytesTransfered)
{
	static HMODULE hArby = LoadLibrary("Arby.dll");
    static ArbySendString_t ArbySendString = (ArbySendString_t)GetProcAddress(hArby, "Arby_SendString");
    static ArbyQueryString_t ArbyQueryString = (ArbyQueryString_t)GetProcAddress(hArby, "Arby_QueryString");
    instronDriver_t *driver = (instronDriver_t*)drvPvt;
    asynStatus status = asynSuccess;
	epicsTimeStamp epicsTS1, epicsTS2;

    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s write.\n", driver->portName);
    asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, numchars,
                "%s write %lu\n", driver->portName, (unsigned long)numchars);
	epicsTimeGetCurrent(&epicsTS1);
    *nbytesTransfered = 0;
    driver->replyData = "";
	if (!driver->connected)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s disconnected:", driver->portName);
            return asynError;
	}
	++(driver->nWriteCalls);
    if (numchars == 0)
	{
        return asynSuccess;
	}
    BOOL res;
    size_t actual = numchars;
    if (data[0] == 'C') {
        res = (*ArbySendString)(driver->deviceId, const_cast<char*>(data));
    } else {
        char reply[256];
        memset(reply, 0, sizeof(reply));
        res = (*ArbyQueryString)(driver->deviceId, const_cast<char*>(data), reply, sizeof(reply));
        if (res) {
            driver->replyData = reply;
            driver->replyData += driver->fakeReadTerminator;
        }
    }
    if (res == 0)
    {
         closeConnection(pasynUser,driver,"Write error");
         epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                       "%s write error", driver->portName);
         return asynError;
    }         
    driver->nWriteBytes += actual;
    *nbytesTransfered = actual;
	epicsTimeGetCurrent(&epicsTS2);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "wrote %lu/%lu chars to %s, return %s.\n", (unsigned long)*nbytesTransfered, (unsigned long)numchars,
                                               driver->portName,
                                               pasynManager->strStatus(status));
	asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s Write took %f timeout was %f\n", 
	          driver->portName, epicsTimeDiffInSeconds(&epicsTS2, &epicsTS1), pasynUser->timeout);
    return status;
}

/// read values from device
static asynStatus readIt(void *drvPvt, asynUser *pasynUser,
    char *data, size_t maxchars, size_t *nbytesTransfered, int *gotEom)
{
    instronDriver_t *driver = (instronDriver_t*)drvPvt;
    int reason = 0;
    asynStatus status = asynSuccess;

    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s read.\n", driver->portName);
    *nbytesTransfered = 0;
    if (gotEom) *gotEom = 0;
	if (!driver->connected)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s disconnected:", driver->portName);
            return asynError;
	}
	++(driver->nReadCalls);
    if (maxchars <= 0) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                  "%s maxchars %d. Why <=0?",driver->portName,(int)maxchars);
        return asynError;
    }
    data[0] = '\0';
//    if (driver->replyData.size() == 0)
//    {
//			closeConnection(pasynUser, driver, "Read error");
//			epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
//				"%s read error", driver->portName);
//			return asynError;
//    }

    strncpy(data, driver->replyData.c_str(), maxchars);
    size_t actual = strlen(data);
    driver->replyData.erase(0, actual);
		asynPrint(pasynUser, ASYN_TRACE_FLOW,
			"read %lu from %s, return %s.\n", (unsigned long)*nbytesTransfered,
			driver->portName,
			pasynManager->strStatus(status));
	if (actual > 0)
	{
        asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, actual,
                   "%s read %d\n", driver->portName, actual);
        driver->nReadBytes += (unsigned long)actual;
    }
    *nbytesTransfered = actual;
    /* If there is room add a null byte */
    if (actual < (int) maxchars)
        data[actual] = 0;
    else
        reason |= ASYN_EOM_CNT;
    if (gotEom) *gotEom = reason;
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "read %lu from %s, return %s.\n", (unsigned long)*nbytesTransfered,
                                               driver->portName,
                                               pasynManager->strStatus(status));
    return status;
}

/// flush device
static asynStatus
flushIt(void *drvPvt,asynUser *pasynUser)
{
    return asynSuccess;
}

static asynOctet asynOctetMethods = { writeIt, readIt, flushIt };

/*
 * asynCommon methods
 */
static const struct asynCommon asynCommonMethods = {
    asynCommonReport,
    asynCommonConnect,
    asynCommonDisconnect
};


/// Create a VISA device.
/// @param[in] portName @copydoc drvAsynInstronArbyConfigureArg0
/// @param[in] portName @copydoc drvAsynInstronArbyConfigureArg1
/// @param[in] priority @copydoc drvAsynInstronArbyConfigureArg2
/// @param[in] noAutoConnect @copydoc drvAsynInstronArbyConfigureArg3
/// @param[in] noProcessEos @copydoc drvAsynInstronArbyConfigureArg4
/// @param[in] readIntTmoMs @copydoc drvAsynInstronArbyConfigureArg5
/// @param[in] termCharIn @copydoc drvAsynInstronArbyConfigureArg6
/// @param[in] deviceSendsEOM @copydoc drvAsynInstronArbyConfigureArg7
epicsShareFunc int
drvAsynInstronArbyConfigure(const char *portName,
                         int deviceId, 
                         const char* fakeReadTerminator, 
                         unsigned int priority,
                         int noAutoConnect,
                         int noProcessEos)
{
    instronDriver_t *driver;
    asynStatus status;
    static int firstTime = 1;

    /*
     * Check arguments
     */
    if (portName == NULL) {
        printf("drvAsynInstronArbyConfigure: Port name missing.\n");
        return -1;
    }
    if (deviceId < 0) {
        printf("drvAsynInstronArbyConfigure: portName information missing.\n");
        return -1;
    }

    /*
     * Perform some one-time-only initializations
     */
    if (firstTime) {
        firstTime = 0;
    }

    /*
     * Create a driver
     */
    driver = (instronDriver_t *)callocMustSucceed(1, sizeof(instronDriver_t), "drvAsyInstronArbyConfigure()");
    driver->connected = false;
    driver->deviceId = deviceId;
    driver->portName = epicsStrDup(portName);
    if (fakeReadTerminator != NULL) {
       char termChar[16];
        epicsStrnRawFromEscaped(termChar, sizeof(termChar), fakeReadTerminator, strlen(fakeReadTerminator));
        driver->fakeReadTerminator = termChar;
    }
    driver->pasynUser = pasynManager->createAsynUser(0,0);

    /*
     *  Link with higher level routines
     */
    driver->common.interfaceType = asynCommonType;
    driver->common.pinterface  = (void *)&asynCommonMethods;
    driver->common.drvPvt = driver;

	if (pasynManager->registerPort(driver->portName,
                                   ASYN_CANBLOCK,
                                   !noAutoConnect,
                                   priority,
                                   0) != asynSuccess) {
        printf("drvAsynInstronArbyConfigure: Can't register myself.\n");
        driverCleanup(driver);
        return -1;
    }
    status = pasynManager->registerInterface(driver->portName,&driver->common);
    if(status != asynSuccess) {
        printf("drvAsynInstronArbyConfigure: Can't register common.\n");
        driverCleanup(driver);
        return -1;
    }
    driver->octet.interfaceType = asynOctetType;
    driver->octet.pinterface  = &asynOctetMethods;
    driver->octet.drvPvt = driver;
    status = pasynOctetBase->initialize(driver->portName,&driver->octet,
                             (noProcessEos ? 0 : 1),(noProcessEos ? 0 : 1),1);
    if(status != asynSuccess) {
        printf("drvAsynInstronArbyConfigure: Can't register octet.\n");
        driverCleanup(driver);
        return -1;
    }
    status = pasynManager->connectDevice(driver->pasynUser,driver->portName,-1);
    if(status != asynSuccess) {
        printf("drvAsynInstronArbyConfigure: connectDevice failed %s\n",driver->pasynUser->errorMessage);
        instronCleanup(driver);
        driverCleanup(driver);
        return -1;
    }

    /*
     * Register for socket cleanup
     */
    epicsAtExit(instronCleanup, driver);
    return 0;
}

/*
 * IOC shell command registration
 */

/// A name for the asyn driver instance we will create e.g. "L0" 
static const iocshArg drvAsynInstronArbyConfigureArg0 = { "portName",iocshArgString}; 
/// VISA resource name to connect to e.g. "GPIB0::3::INSTR" or "COM10"
static const iocshArg drvAsynInstronArbyConfigureArg1 = { "deviceId",iocshArgInt};
static const iocshArg drvAsynInstronArbyConfigureArg2 = { "fakeReadTerminator",iocshArgString};
/// Driver priority 
static const iocshArg drvAsynInstronArbyConfigureArg3 = { "priority",iocshArgInt};
/// Should the driver automatically connect to the device (0=yes) 
static const iocshArg drvAsynInstronArbyConfigureArg4 = { "noAutoConnect",iocshArgInt};
/// Should the driver interpose layer be called for EOS (termination) character processing (0=yes)
/// If you have no termination character specified to asyn, then passing 1 (=no) may improve efficiency 
static const iocshArg drvAsynInstronArbyConfigureArg5 = { "noProcessEos",iocshArgInt};
/// internal read timeout (ms) used instead of a zero timeout immediate read. Stream device will use such

static const iocshArg *drvAsynInstronArbyConfigureArgs[] = {
    &drvAsynInstronArbyConfigureArg0, &drvAsynInstronArbyConfigureArg1, &drvAsynInstronArbyConfigureArg2,
    &drvAsynInstronArbyConfigureArg3, &drvAsynInstronArbyConfigureArg4, &drvAsynInstronArbyConfigureArg5

};

static const iocshFuncDef drvAsynInstronArbyConfigureFuncDef =
                      {"drvAsynInstronArbyConfigure",sizeof(drvAsynInstronArbyConfigureArgs)/sizeof(iocshArg*),drvAsynInstronArbyConfigureArgs};

static void drvAsynInstronArbyConfigureCallFunc(const iocshArgBuf *args)
{
    drvAsynInstronArbyConfigure(args[0].sval, args[1].ival, args[2].sval, args[3].ival,
                             args[4].ival, args[5].ival);
}

extern "C"
{

static void
drvAsynInstronArbyConfigureRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        iocshRegister(&drvAsynInstronArbyConfigureFuncDef,drvAsynInstronArbyConfigureCallFunc);
        firstTime = 0;
    }
}

epicsExportRegistrar(drvAsynInstronArbyConfigureRegister);

}
