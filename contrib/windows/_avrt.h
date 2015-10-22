
#include <Windows.h>
#pragma comment(lib, "Avrt.lib")

typedef enum _TASK_ID {
    Audio = 0,
    Capture,
    Distribution,
    Games,
    LowLatency,
    Playback,
    ProAudio,
    WindowManager,
    InvalidTaskId
} TASK_ID;

typedef struct _TASK {
    TASK_ID    TaskId;
    LPCWSTR    TaskName;
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
    { InvalidTaskId,    L"InvalidTaskId" },
};

static const ULONG NumTasks = sizeof(Tasks)/sizeof(TASK);

static __inline
BOOL
TaskIdToTaskName(_In_ TASK_ID TaskId, _Out_ LPCWSTR *TaskName)
{
    if (TaskId >= InvalidTaskId)
        return FALSE;
    *TaskName = Tasks[TaskId].TaskName;
    return TRUE;
}

/* vim: set ts=8 sw=4 sts=4 tw=80 et:                                         */
