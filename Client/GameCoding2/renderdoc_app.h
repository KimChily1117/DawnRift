#pragma once

#if !defined(RENDERDOC_NO_STDINT)
#include <stdint.h>
#endif

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(_MSC_VER)
#define RENDERDOC_CC __cdecl
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__sun__) || defined(__OpenBSD__)
#define RENDERDOC_CC
#elif defined(__APPLE__)
#define RENDERDOC_CC
#else
#error "Unknown platform"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define RENDERDOC_ShaderDebugMagicValue_struct                                                                         \
    {                                                                                                                  \
        0xeab25520, 0x6670, 0x4865, 0x84, 0x29, 0x6c, 0x8, 0x51, 0x54, 0x00, 0xff                                      \
    }

#define RENDERDOC_ShaderDebugMagicValue_bytearray                                                                      \
    {                                                                                                                  \
        0x20, 0x55, 0xb2, 0xea, 0x70, 0x66, 0x65, 0x48, 0x84, 0x29, 0x6c, 0x8, 0x51, 0x54, 0x00, 0xff                  \
    }

#define RENDERDOC_ShaderDebugMagicValue_truncated 0x48656670eab25520ULL

#define RENDERDOC_APIObjectAnnotationHelper 0xfbb3b337b664d0adULL

    typedef enum RENDERDOC_CaptureOption
    {
        eRENDERDOC_Option_AllowVSync = 0,

        eRENDERDOC_Option_AllowFullscreen = 1,

        eRENDERDOC_Option_APIValidation = 2,
        eRENDERDOC_Option_DebugDeviceMode = 2,

        eRENDERDOC_Option_CaptureCallstacks = 3,

        eRENDERDOC_Option_CaptureCallstacksOnlyDraws = 4,
        eRENDERDOC_Option_CaptureCallstacksOnlyActions = 4,

        eRENDERDOC_Option_DelayForDebugger = 5,

        eRENDERDOC_Option_VerifyBufferAccess = 6,

        eRENDERDOC_Option_VerifyMapWrites = eRENDERDOC_Option_VerifyBufferAccess,

        eRENDERDOC_Option_HookIntoChildren = 7,

        eRENDERDOC_Option_RefAllResources = 8,

        eRENDERDOC_Option_SaveAllInitials = 9,

        eRENDERDOC_Option_CaptureAllCmdLists = 10,

        eRENDERDOC_Option_DebugOutputMute = 11,

        eRENDERDOC_Option_AllowUnsupportedVendorExtensions = 12,

        eRENDERDOC_Option_SoftMemoryLimit = 13,
    } RENDERDOC_CaptureOption;

    typedef int(RENDERDOC_CC* pRENDERDOC_SetCaptureOptionU32)(RENDERDOC_CaptureOption opt, uint32_t val);
    typedef int(RENDERDOC_CC* pRENDERDOC_SetCaptureOptionF32)(RENDERDOC_CaptureOption opt, float val);

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_GetCaptureOptionU32)(RENDERDOC_CaptureOption opt);

    typedef float(RENDERDOC_CC* pRENDERDOC_GetCaptureOptionF32)(RENDERDOC_CaptureOption opt);

    typedef enum RENDERDOC_InputButton
    {
        // '0' - '9' matches ASCII values
        eRENDERDOC_Key_0 = 0x30,
        eRENDERDOC_Key_1 = 0x31,
        eRENDERDOC_Key_2 = 0x32,
        eRENDERDOC_Key_3 = 0x33,
        eRENDERDOC_Key_4 = 0x34,
        eRENDERDOC_Key_5 = 0x35,
        eRENDERDOC_Key_6 = 0x36,
        eRENDERDOC_Key_7 = 0x37,
        eRENDERDOC_Key_8 = 0x38,
        eRENDERDOC_Key_9 = 0x39,

        // 'A' - 'Z' matches ASCII values
        eRENDERDOC_Key_A = 0x41,
        eRENDERDOC_Key_B = 0x42,
        eRENDERDOC_Key_C = 0x43,
        eRENDERDOC_Key_D = 0x44,
        eRENDERDOC_Key_E = 0x45,
        eRENDERDOC_Key_F = 0x46,
        eRENDERDOC_Key_G = 0x47,
        eRENDERDOC_Key_H = 0x48,
        eRENDERDOC_Key_I = 0x49,
        eRENDERDOC_Key_J = 0x4A,
        eRENDERDOC_Key_K = 0x4B,
        eRENDERDOC_Key_L = 0x4C,
        eRENDERDOC_Key_M = 0x4D,
        eRENDERDOC_Key_N = 0x4E,
        eRENDERDOC_Key_O = 0x4F,
        eRENDERDOC_Key_P = 0x50,
        eRENDERDOC_Key_Q = 0x51,
        eRENDERDOC_Key_R = 0x52,
        eRENDERDOC_Key_S = 0x53,
        eRENDERDOC_Key_T = 0x54,
        eRENDERDOC_Key_U = 0x55,
        eRENDERDOC_Key_V = 0x56,
        eRENDERDOC_Key_W = 0x57,
        eRENDERDOC_Key_X = 0x58,
        eRENDERDOC_Key_Y = 0x59,
        eRENDERDOC_Key_Z = 0x5A,

        eRENDERDOC_Key_NonPrintable = 0x100,

        eRENDERDOC_Key_Divide,
        eRENDERDOC_Key_Multiply,
        eRENDERDOC_Key_Subtract,
        eRENDERDOC_Key_Plus,

        eRENDERDOC_Key_F1,
        eRENDERDOC_Key_F2,
        eRENDERDOC_Key_F3,
        eRENDERDOC_Key_F4,
        eRENDERDOC_Key_F5,
        eRENDERDOC_Key_F6,
        eRENDERDOC_Key_F7,
        eRENDERDOC_Key_F8,
        eRENDERDOC_Key_F9,
        eRENDERDOC_Key_F10,
        eRENDERDOC_Key_F11,
        eRENDERDOC_Key_F12,

        eRENDERDOC_Key_Home,
        eRENDERDOC_Key_End,
        eRENDERDOC_Key_Insert,
        eRENDERDOC_Key_Delete,
        eRENDERDOC_Key_PageUp,
        eRENDERDOC_Key_PageDn,

        eRENDERDOC_Key_Backspace,
        eRENDERDOC_Key_Tab,
        eRENDERDOC_Key_PrtScrn,
        eRENDERDOC_Key_Pause,

        eRENDERDOC_Key_Max,
    } RENDERDOC_InputButton;

    typedef void(RENDERDOC_CC* pRENDERDOC_SetFocusToggleKeys)(RENDERDOC_InputButton* keys, int num);

    typedef void(RENDERDOC_CC* pRENDERDOC_SetCaptureKeys)(RENDERDOC_InputButton* keys, int num);

    typedef enum RENDERDOC_OverlayBits
    {
        eRENDERDOC_Overlay_Enabled = 0x1,

        eRENDERDOC_Overlay_FrameRate = 0x2,

        // Show the current frame number
        eRENDERDOC_Overlay_FrameNumber = 0x4,

        eRENDERDOC_Overlay_CaptureList = 0x8,

        // Default values for the overlay mask
        eRENDERDOC_Overlay_Default = (eRENDERDOC_Overlay_Enabled | eRENDERDOC_Overlay_FrameRate |
                                      eRENDERDOC_Overlay_FrameNumber | eRENDERDOC_Overlay_CaptureList),

        // Enable all bits
        eRENDERDOC_Overlay_All = 0x7ffffff,

        // Disable all bits
        eRENDERDOC_Overlay_None = 0,
    } RENDERDOC_OverlayBits;

    // returns the overlay bits that have been set
    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_GetOverlayBits)(void);
    // sets the overlay bits with an and & or mask
    typedef void(RENDERDOC_CC* pRENDERDOC_MaskOverlayBits)(uint32_t And, uint32_t Or);

    typedef void(RENDERDOC_CC* pRENDERDOC_RemoveHooks)(void);

    typedef pRENDERDOC_RemoveHooks pRENDERDOC_Shutdown;

    typedef void(RENDERDOC_CC* pRENDERDOC_UnloadCrashHandler)(void);

    typedef void(RENDERDOC_CC* pRENDERDOC_SetCaptureFilePathTemplate)(const char* pathtemplate);

    typedef const char*(RENDERDOC_CC* pRENDERDOC_GetCaptureFilePathTemplate)(void);

    typedef pRENDERDOC_SetCaptureFilePathTemplate pRENDERDOC_SetLogFilePathTemplate;
    typedef pRENDERDOC_GetCaptureFilePathTemplate pRENDERDOC_GetLogFilePathTemplate;

    // returns the number of captures that have been made
    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_GetNumCaptures)(void);

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_GetCapture)(uint32_t idx, char* filename, uint32_t* pathlength,
                                                          uint64_t* timestamp);

    typedef void(RENDERDOC_CC* pRENDERDOC_SetCaptureFileComments)(const char* filePath, const char* comments);

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_IsTargetControlConnected)(void);

    typedef pRENDERDOC_IsTargetControlConnected pRENDERDOC_IsRemoteAccessConnected;

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_LaunchReplayUI)(uint32_t connectTargetControl, const char* cmdline);

    typedef void(RENDERDOC_CC* pRENDERDOC_GetAPIVersion)(int* major, int* minor, int* patch);

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_ShowReplayUI)(void);

    typedef void* RENDERDOC_DevicePointer;

    typedef void* RENDERDOC_WindowHandle;

#define RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(inst) (*((void**)(inst)))

    typedef void(RENDERDOC_CC* pRENDERDOC_SetActiveWindow)(RENDERDOC_DevicePointer device,
                                                           RENDERDOC_WindowHandle wndHandle);

    typedef void(RENDERDOC_CC* pRENDERDOC_TriggerCapture)(void);

    typedef void(RENDERDOC_CC* pRENDERDOC_TriggerMultiFrameCapture)(uint32_t numFrames);

    typedef void(RENDERDOC_CC* pRENDERDOC_StartFrameCapture)(RENDERDOC_DevicePointer device,
                                                             RENDERDOC_WindowHandle wndHandle);

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_IsFrameCapturing)(void);

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_EndFrameCapture)(RENDERDOC_DevicePointer device,
                                                               RENDERDOC_WindowHandle wndHandle);

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_DiscardFrameCapture)(RENDERDOC_DevicePointer device,
                                                                   RENDERDOC_WindowHandle wndHandle);

    typedef void(RENDERDOC_CC* pRENDERDOC_SetCaptureTitle)(const char* title);

    typedef enum RENDERDOC_AnnotationType
    {
        eRENDERDOC_Empty,
        eRENDERDOC_Bool,
        eRENDERDOC_Int32,
        eRENDERDOC_UInt32,
        eRENDERDOC_Int64,
        eRENDERDOC_UInt64,
        eRENDERDOC_Float,
        eRENDERDOC_Double,
        eRENDERDOC_String,
        eRENDERDOC_APIObject,
        eRENDERDOC_AnnotationMax = 0x7FFFFFFF,
    } RENDERDOC_AnnotationType;

    // a union with vector annotation value data
    typedef union RENDERDOC_AnnotationVectorValue
    {
        bool boolean[4];
        int32_t int32[4];
        int64_t int64[4];
        uint32_t uint32[4];
        uint64_t uint64[4];
        float float32[4];

        double float64[4];
    } RENDERDOC_AnnotationVectorValue;

    // a union with scalar annotation value data
    typedef union RENDERDOC_AnnotationValue
    {
        bool boolean;
        int32_t int32;
        int64_t int64;
        uint32_t uint32;
        uint64_t uint64;
        float float32;

        double float64;

        RENDERDOC_AnnotationVectorValue vector;

        const char* string;
        void* apiObject;
    } RENDERDOC_AnnotationValue;

    typedef struct RENDERDOC_GLResourceReference
    {
        uint32_t identifier;
        uint32_t name;
    } GLResourceReference;

#ifdef __cplusplus
    struct RDGLObjectHelper
    {
        RENDERDOC_GLResourceReference gl;

        RDGLObjectHelper(uint32_t identifier, uint32_t name)
        {
            gl.identifier = identifier;
            gl.name = name;
        }

        operator RENDERDOC_GLResourceReference*()
        {
            return &gl;
        }
    };

    struct RDAnnotationHelper
    {
        RENDERDOC_AnnotationValue val;

        RDAnnotationHelper(bool b)
        {
            val.boolean = b;
        }

        RDAnnotationHelper(int32_t i)
        {
            val.int32 = i;
        }

        RDAnnotationHelper(int64_t i)
        {
            val.int64 = i;
        }

        RDAnnotationHelper(uint32_t i)
        {
            val.uint32 = i;
        }

        RDAnnotationHelper(uint64_t i)
        {
            val.uint64 = i;
        }

        RDAnnotationHelper(float f)
        {
            val.float32 = f;
        }

        RDAnnotationHelper(double d)
        {
            val.float64 = d;
        }

        RDAnnotationHelper(const char* s)
        {
            val.string = s;
        }

        operator RENDERDOC_AnnotationValue*()
        {
            return &val;
        }
    };
#endif

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_SetObjectAnnotation)(RENDERDOC_DevicePointer device, void* object,
                                                                   const char* key, RENDERDOC_AnnotationType valueType,
                                                                   uint32_t valueVectorWidth,
                                                                   const RENDERDOC_AnnotationValue* value);

    typedef uint32_t(RENDERDOC_CC* pRENDERDOC_SetCommandAnnotation)(RENDERDOC_DevicePointer device,
                                                                    void* queueOrCommandBuffer, const char* key,
                                                                    RENDERDOC_AnnotationType valueType,
                                                                    uint32_t valueVectorWidth,
                                                                    const RENDERDOC_AnnotationValue* value);

    typedef enum RENDERDOC_Version
    {
        eRENDERDOC_API_Version_1_0_0 = 10000,
        eRENDERDOC_API_Version_1_0_1 = 10001,
        eRENDERDOC_API_Version_1_0_2 = 10002,
        eRENDERDOC_API_Version_1_1_0 = 10100,
        eRENDERDOC_API_Version_1_1_1 = 10101,
        eRENDERDOC_API_Version_1_1_2 = 10102,
        eRENDERDOC_API_Version_1_2_0 = 10200,
        eRENDERDOC_API_Version_1_3_0 = 10300,
        eRENDERDOC_API_Version_1_4_0 = 10400,
        eRENDERDOC_API_Version_1_4_1 = 10401,
        eRENDERDOC_API_Version_1_4_2 = 10402,
        eRENDERDOC_API_Version_1_5_0 = 10500,
        eRENDERDOC_API_Version_1_6_0 = 10600,
        eRENDERDOC_API_Version_1_7_0 = 10700,
    } RENDERDOC_Version;

    typedef struct RENDERDOC_API_1_7_0
    {
        pRENDERDOC_GetAPIVersion GetAPIVersion;

        pRENDERDOC_SetCaptureOptionU32 SetCaptureOptionU32;
        pRENDERDOC_SetCaptureOptionF32 SetCaptureOptionF32;

        pRENDERDOC_GetCaptureOptionU32 GetCaptureOptionU32;
        pRENDERDOC_GetCaptureOptionF32 GetCaptureOptionF32;

        pRENDERDOC_SetFocusToggleKeys SetFocusToggleKeys;
        pRENDERDOC_SetCaptureKeys SetCaptureKeys;

        pRENDERDOC_GetOverlayBits GetOverlayBits;
        pRENDERDOC_MaskOverlayBits MaskOverlayBits;

        union
        {
            pRENDERDOC_Shutdown Shutdown;
            pRENDERDOC_RemoveHooks RemoveHooks;
        };

        pRENDERDOC_UnloadCrashHandler UnloadCrashHandler;

        union
        {
            // deprecated name
            pRENDERDOC_SetLogFilePathTemplate SetLogFilePathTemplate;
            // current name
            pRENDERDOC_SetCaptureFilePathTemplate SetCaptureFilePathTemplate;
        };

        union
        {
            // deprecated name
            pRENDERDOC_GetLogFilePathTemplate GetLogFilePathTemplate;
            // current name
            pRENDERDOC_GetCaptureFilePathTemplate GetCaptureFilePathTemplate;
        };

        pRENDERDOC_GetNumCaptures GetNumCaptures;
        pRENDERDOC_GetCapture GetCapture;

        pRENDERDOC_TriggerCapture TriggerCapture;

        union
        {
            // deprecated name
            pRENDERDOC_IsRemoteAccessConnected IsRemoteAccessConnected;
            // current name
            pRENDERDOC_IsTargetControlConnected IsTargetControlConnected;
        };

        pRENDERDOC_LaunchReplayUI LaunchReplayUI;

        pRENDERDOC_SetActiveWindow SetActiveWindow;

        pRENDERDOC_StartFrameCapture StartFrameCapture;
        pRENDERDOC_IsFrameCapturing IsFrameCapturing;
        pRENDERDOC_EndFrameCapture EndFrameCapture;

        // new function in 1.1.0
        pRENDERDOC_TriggerMultiFrameCapture TriggerMultiFrameCapture;

        // new function in 1.2.0
        pRENDERDOC_SetCaptureFileComments SetCaptureFileComments;

        // new function in 1.4.0
        pRENDERDOC_DiscardFrameCapture DiscardFrameCapture;

        // new function in 1.5.0
        pRENDERDOC_ShowReplayUI ShowReplayUI;

        // new function in 1.6.0
        pRENDERDOC_SetCaptureTitle SetCaptureTitle;

        // new functions in 1.7.0
        pRENDERDOC_SetObjectAnnotation SetObjectAnnotation;
        pRENDERDOC_SetCommandAnnotation SetCommandAnnotation;
    } RENDERDOC_API_1_7_0;

    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_0_0;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_0_1;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_0_2;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_1_0;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_1_1;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_1_2;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_2_0;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_3_0;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_4_0;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_4_1;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_4_2;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_5_0;
    typedef RENDERDOC_API_1_7_0 RENDERDOC_API_1_6_0;

    typedef int(RENDERDOC_CC* pRENDERDOC_GetAPI)(RENDERDOC_Version version, void** outAPIPointers);

#ifdef __cplusplus
}
#endif
