
#define LINEDEBUG

#include "callbacks.h"
#include "amaterasu.h"

static void AddPidToHandleArr(_In_ HANDLE PID) {

    KIRQL OldIrql;
    DbgPrint("Pre HandleArrSize: %ul\n", Amaterasu.HandleArrSize);

    KeAcquireSpinLock(&Amaterasu.HandleArrLock, &OldIrql);

    if(Amaterasu.HandleArrSize < 1024) {
        Amaterasu.HandleArr[Amaterasu.HandleArrSize] = PID;
        Amaterasu.HandleArrSize++;
    }
    DbgPrint("Post HandleArrSize: %ul\n", Amaterasu.HandleArrSize);
    KeReleaseSpinLock(&Amaterasu.HandleArrLock, OldIrql);
}

static BOOLEAN AreWeTrackingIt(_In_ HANDLE PID) {

    BOOLEAN Ret = FALSE;
    NTSTATUS Status;
    PEPROCESS eProc;
    PUNICODE_STRING ImageName;

    if(Amaterasu.HandleArrSize) {
        for(SIZE_T i = 0; i < Amaterasu.HandleArrSize; i++) {
            if(PID == Amaterasu.HandleArr[i]) {
                return TRUE;
            }
        }
        return FALSE;
    } 

    Status = PsLookupProcessByProcessId(PID, &eProc);
    if(NT_SUCCESS(Status)) {
        Status = SeLocateProcessImageName(eProc, &ImageName);
        if(NT_SUCCESS(Status)) {
            if (ImageName && ImageName->Buffer) {
                if (wcsstr(ImageName->Buffer, Amaterasu.DriverSettings.TargetName)) {
                    AddPidToHandleArr(PID);
                    Ret = TRUE;
                }
            }
        }
        //ObDereferenceObject(eProc); VER DEPOIS
        RtlFreeUnicodeString(ImageName);
    }
    
    return Ret;
}

FLT_PREOP_CALLBACK_STATUS AmaterasuDefaultPreCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
) {

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    HANDLE PID;

    PID = (HANDLE)FltGetRequestorProcessId(Data);
    if(AreWeTrackingIt(PID)) {
        if (Amaterasu.InfoList) {
            InfoListAppend(Amaterasu.InfoList, Data, INFO_FS);
        }
    }

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS AmaterasuPost(
	PFLT_CALLBACK_DATA Data,
	PCFLT_RELATED_OBJECTS FltObjects,
	PVOID* CompletionContext,
	FLT_POST_OPERATION_FLAGS Flags) {

	return FLT_POSTOP_FINISHED_PROCESSING;
}

void AmaterasuLoadImageCallback(
	PUNICODE_STRING FullImageName,
	HANDLE ProcessId,
	PIMAGE_INFO ImageInfo
) {

	LOAD_IMAGE_DATA LoadImageData;

    if(AreWeTrackingIt(ProcessId)) {

        LoadImageData.FullImageName = FullImageName;
        LoadImageData.ProcessId = ProcessId;
        LoadImageData.ImageInfo = ImageInfo;

        InfoListAppend(Amaterasu.InfoList, &LoadImageData, INFO_LOAD);
        line();
    }

	return;
}

NTSTATUS AmaterasuRegCallback(
	PVOID CallbackContext,
	PVOID RegNotifyClass,
	PVOID RegStruct
){

	NTSTATUS Status;
    HANDLE PID;
	REG_INFO_DATA RegInfoData;
    REG_NOTIFY_CLASS RegClass;

    UNREFERENCED_PARAMETER(CallbackContext);

	Status = STATUS_SUCCESS;

    RegClass = (REG_NOTIFY_CLASS)RegNotifyClass; 

    if (RegClass != RegNtSetValueKey && RegClass != RegNtDeleteValueKey) {
        return Status;
    }

    PID = PsGetCurrentProcessId();

    if(AreWeTrackingIt(PID)) {

        RegInfoData.RegNotifyClass = RegClass;
        RegInfoData.RegStruct = RegStruct;

        Status = InfoListAppend(Amaterasu.InfoList, &RegInfoData, INFO_REG);
        if(!NT_SUCCESS(Status)) {
            line();
            Status = STATUS_UNSUCCESSFUL;
        }
        line();
    }

	return Status;
}

void AmaterasuProcCallback(_In_ HANDLE PPID, _In_ HANDLE PID, _In_ BOOLEAN Active) {

	NTSTATUS Status;
	IDENTIFIER IDs;

    if(!AreWeTrackingIt(PID)) {
        if(AreWeTrackingIt(PPID)) {
             AddPidToHandleArr(PID);
        } else {
            return;
        }
    }

	IDs.Active = Active;
	IDs.PPID = PPID;
	IDs.Id.PID = PID;
	IDs.isThread = FALSE;
	
	Status = InfoListAppend(Amaterasu.InfoList, &IDs, INFO_PROC);
    line();
	if (!NT_SUCCESS(Status)) {
        line();
		return;
	}

    line();
}

void AmaterasuThreadCallback(_In_ HANDLE PPID, _In_ HANDLE TID, _In_ BOOLEAN Active) {

	NTSTATUS Status;
	IDENTIFIER IDs;

    if(AreWeTrackingIt(PPID)) {

        IDs.Active = Active;
        IDs.PPID = PPID;
        IDs.Id.TID = TID;
        IDs.isThread = TRUE;

        Status = InfoListAppend(Amaterasu.InfoList, &IDs, INFO_PROC);
        if (!NT_SUCCESS(Status)) {
            line();
            return;
        }
        line();
    }
}
