
#include <Windows.h>
#pragma comment(lib, "Avrt.lib")

enum Tasks {
    Audio = 0,
    Capture,
    Distribution,
    Games,
    LowLatency,
    Playback,
    ProAudio,
    WindowManager
};

typedef struct _TASK {
    DWORD   TaskId;
    LPCWSTR TaskName;
} TASK, *PTASK;

static TASK Tasks[] = {
    { Audio,            L"Audio" },
    { Capture,          L"Capture" },
    { Distribution,     L"Distribution" },
    { Games,            L"Games" },
    { LowLatency,       L"LowLatency" },
    { Playback,         L"Playback" },
    { ProAudio,         L"ProAudio" },
    { WindowManager,    L"WindowManager" },
};

static const DWORD NumTasks = sizeof(Tasks)/sizeof(TASK);

static __inline
BOOL
TaskIdToTaskName(_In_ DWORD TaskId, _Out_ LPCWSTR *TaskName)
{
    if (TaskId < 0 || TaskId > NumTasks-1)
        return FALSE;
    *TaskName = Tasks[TaskId].TaskName;
    return TRUE;
}

/* vim: set ts=8 sw=4 sts=4 tw=80 et:                                         */
