#ifndef DATA_H
#define DATA_H

typedef struct Data {

    ULONG TimeStampSize;
    ULONG ActionSize;

    UCHAR TimeStamp[128];
    UCHAR Action[64];

    PPROC_DATA ProcData;
    PFILE_DATA FileData;

} DATA, *PDATA;

#endif  /* DATA_H */
