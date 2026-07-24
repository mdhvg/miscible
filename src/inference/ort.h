// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "onnxruntime/core/session/onnxruntime_error_code.h"

#define ORTResult(st)                                                                                                             \
    res = GenResult(!st, Domain_Inference_ONNX, inf_ctx.api->GetErrorCode(st), inference_ort_err(inf_ctx.api->GetErrorCode(st))); \
    CheckAndClearResult(res);

#include "base/base_core.h"
global_v inline const char *inference_ort_err(OrtErrorCode code)
{
    switch (code)
    {
    case ORT_OK:
        return "Success. No error occurred.";
        break;
    case ORT_FAIL:
        return "Generic failure that does not map to a more specific error code. Consult the error message for details.";
        break;
    case ORT_INVALID_ARGUMENT:
        return "A caller-supplied argument was invalid (e.g. NULL pointer, out-of-range value, mismatched shape/rank, or bad configuration).";
        break;
    case ORT_NO_SUCHFILE:
        return "A required file (such as a model file) does not exist.";
        break;
    case ORT_NO_MODEL:
        return "Legacy/unused but retained for ABI compatibility. Historically returned when a model could not be found by name in the ONNX Runtime Server (removed in 2022).";
        break;
    case ORT_ENGINE_ERROR:
        return "A hardware accelerator or backend engine reported a failure (e.g. a device crash or other device-level error).";
        break;
    case ORT_RUNTIME_EXCEPTION:
        return "A generic runtime exception was caught. The error message is the primary source of detail.";
        break;
    case ORT_INVALID_PROTOBUF:
        return "Protobuf parsing or serialization failed.";
        break;
    case ORT_MODEL_LOADED:
        return "Invalid session state for the requested operation. Despite the name, this code does not mean \" success, model loaded \"; it is returned when the session is in the wrong state for the requested call (e.g. a model is already loaded, the session is already initialized, or no model has been loaded yet). The name is historical and is retained for ABI compatibility; consult the error message for the specific condition.";
        break;
    case ORT_NOT_IMPLEMENTED:
        return "The requested functionality is not implemented in this build.";
        break;
    case ORT_INVALID_GRAPH:
        return "The model graph is structurally invalid (e.g. recursive function definitions, invalid tensor dimensions, or malformed nodes).";
        break;
    case ORT_EP_FAIL:
        return "An execution provider reported a generic failure.";
        break;
    case ORT_MODEL_LOAD_CANCELED:
        return "Model loading or session initialization was canceled at the caller's request.";
        break;
    case ORT_MODEL_REQUIRES_COMPILATION:
        return "The model requires compilation by an execution provider, but compilation was disabled via session options.";
        break;
    case ORT_NOT_FOUND:
        return "A requested resource could not be found.";
        break;
    }
}
