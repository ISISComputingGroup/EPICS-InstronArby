#!../../bin/windows-x64/instronArbyTest

## You may have to change instronArbyTest to something else
## everywhere it appears in this file

# Increase this if you get <<TRUNCATED>> or discarded messages warnings in your errlog output
errlogInit2(65536, 256)

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/instronArbyTest.dbd"
instronArbyTest_registerRecordDeviceDriver pdbbase

drvAsynInstronArbyConfigure("instron", 0, "\r\n")


# trace flow
#asynSetTraceMask("instron",0,0x11) 
# trace I/O
#asynSetTraceMask("instron",0,0x9) 
#asynSetTraceIOMask("instron",0,0x2)

epicsEnvSet ("STREAM_PROTOCOL_PATH", "$(TOP)/data")

## Load our record instances
dbLoadRecords("db/InstronArbyTest.db","P=$(MYPVPREFIX),Q=INSTRON:,PORT=instron")
dbLoadRecords("$(ASYN)/db/asynRecord.db","P=$(MYPVPREFIX),R=INSTRON:ASYNREC,PORT=instron,ADDR=0,OMAX=80,IMAX=80")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=faa59Host"

