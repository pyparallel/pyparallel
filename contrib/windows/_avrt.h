
#include <Windows.h>
#include <wchar.h>
#pragma comment(lib, "Avrt.lib")

typedef enum _TASK_ID {
    Audio = 1,
    Capture,
    Distribution,
    Games,
    LowLatency,
    Playback,
    ProAudio,
    WindowManager,
    InvalidTaskId
} TASK_ID, *PTASK_ID;

typedef struct _TASK {
    TASK_ID    TaskId;
    LPCWSTR    TaskName;
    LPCSTR     TaskNameA;
} TASK, *PTASK;

static TASK Tasks[] = {
    { Audio,            L"Audio",           "Audio"             },
    { Capture,          L"Capture",         "Capture"           },
    { Distribution,     L"Distribution",    "Distribution"      },
    { Games,            L"Games",           "Games"             },
    { LowLatency,       L"Low Latency",     "Low Latency"       },
    { Playback,         L"Playback",        "Playback"          },
    { ProAudio,         L"Pro Audio" ,      "Pro Audio"         },
    { WindowManager,    L"Window Manager",  "Window Manager"    },
    { InvalidTaskId,    L"InvalidTaskId",   "InvalidTaskId"     },
};

static const ULONG NumTasks = (
    sizeof(Tasks) /
    sizeof(TASK)
);

static const size_t MaxTaskNameLength = (
    sizeof(L"Window Manager") /
    sizeof(wchar_t)
);

static const size_t MaxTaskNameLengthA = (
    sizeof("Window Manager") /
    sizeof(char)
);

static __inline
BOOL
TaskIdToTaskName(_In_ TASK_ID TaskId, _Out_ LPCWSTR* TaskName)
{
    if (TaskId <= 0 || TaskId >= InvalidTaskId)
        return FALSE;
    *TaskName = Tasks[TaskId-1].TaskName;
    return TRUE;
}

static __inline
BOOL
TaskNameToTaskId(
    _In_ LPCWSTR TaskName,
    _Out_ PTASK_ID TaskId,
    _Out_opt_ LPCWSTR* pTaskName
)
{
    UINT i, j;
    for (i = 0, j = 1; j < NumTasks; i++, j++) {
        if (!wcsncmp(TaskName, Tasks[i].TaskName, MaxTaskNameLength)) {
            *TaskId = (TASK_ID)j;
            if (pTaskName)
                *pTaskName = Tasks[i].TaskName;
            return TRUE;
        }
    }
    return FALSE;
}

static __inline
BOOL
TaskNameAToTaskId(
    _In_ LPCSTR TaskNameA,
    _Out_ PTASK_ID TaskId,
    _Out_opt_ LPCWSTR* pTaskName
)
{
    UINT i, j;
    for (i = 0, j = 1; j < NumTasks; i++, j++) {
        if (!strncmp(TaskNameA, Tasks[i].TaskNameA, MaxTaskNameLengthA)) {
            *TaskId = (TASK_ID)j;
            if (pTaskName)
                *pTaskName = Tasks[i].TaskName;
            return TRUE;
        }
    }
    return FALSE;
}

/* vim: set ts=8 sw=4 sts=4 tw=80 et:                                         */
