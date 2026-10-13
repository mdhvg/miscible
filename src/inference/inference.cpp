// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "jsmn.h"
#include "onnxruntime/core/session/onnxruntime_c_api.h"
#if OS_WIN32
#include "onnxruntime/core/providers/dml/dml_provider_factory.h"
#endif

#include "ortx_types.h"
#include "ortx_tokenizer.h"

#include "config.h"
#include "base/log.h"
#include "os/os_inc.h"
#include "base/array.h"
#include "ui/ui_core.h"
#include "app/miscible.h"
#include "inference/ort.h"
#include "inference/ops.h"
#include "inference/model.h"
#include "inference/inference.h"

// Dynamic files
#include "_dynamic/manifest.cpp"

struct InferenceContext
{
    Arena *arena;
    const OrtApi *api;
#if OS_WIN32
    const OrtDmlApi *dml_api;
#endif
    OrtEnv *env;
    OrtSessionOptions *session_opt;

    // Text model
    TextModelConfig text_cfg;
    // Vision model
    VisionModelConfig vision_cfg;

    Mutex session_lock;
    InferenceState state;

    OrtSession *text_sess;
    OrtSession *vision_sess;

#if OS_WIN32
    U32 hardware_id;
#elif OS_LINUX
#endif
    InferenceHardwareArr hardware;
};

InferenceContext inf_ctx = {.api = 0};

void inference_init()
{
    arena_alloc(MB(1), inf_ctx.arena);
    inf_ctx.api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    Assert(inf_ctx.api, "Failed to fetch ONNX Runtime API table");
    os_mutex_init(&inf_ctx.session_lock);
}

Result inference_tokenizer_init(Str8 model_base)
{
    os_mutex_init(&inf_ctx.text_cfg.tokenizer_lock);
    extError_t ortx_err = OrtxCreateTokenizer(&inf_ctx.text_cfg.tokenizer, CStrCast(model_base));
    return GenResult(ortx_err == kOrtxOK, Domain_Inference_ONNX, ortx_err, OrtxGetLastErrorMessage());
}

void inference_close()
{
    ins_atomic_u32_eval_assign(&inf_ctx.state, InferenceState_Uninitialized);

    os_mutex_lock(&inf_ctx.text_cfg.tokenizer_lock);
    OrtxDisposeOnly(inf_ctx.text_cfg.tokenizer);
    os_mutex_unlock(&inf_ctx.text_cfg.tokenizer_lock);
    os_mutex_destroy(&inf_ctx.text_cfg.tokenizer_lock);

    inf_ctx.api->SessionOptionsSetLoadCancellationFlag(inf_ctx.session_opt, 1);

    os_mutex_lock(&inf_ctx.session_lock);
    if (inf_ctx.text_sess) inf_ctx.api->ReleaseSession(inf_ctx.text_sess);
    if (inf_ctx.vision_sess) inf_ctx.api->ReleaseSession(inf_ctx.vision_sess);
    os_mutex_unlock(&inf_ctx.session_lock);
    os_mutex_destroy(&inf_ctx.session_lock);

    if (inf_ctx.session_opt) inf_ctx.api->ReleaseSessionOptions(inf_ctx.session_opt);
    if (inf_ctx.env) inf_ctx.api->ReleaseEnv(inf_ctx.env);
}

void inference_create_session_opt()
{
    OrtStatus *status = inf_ctx.api->CreateSessionOptions(&inf_ctx.session_opt);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    status = inf_ctx.api->DisableMemPattern(inf_ctx.session_opt);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
    status = inf_ctx.api->SetSessionExecutionMode(inf_ctx.session_opt, ORT_SEQUENTIAL);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    status = inf_ctx.api->AddSessionConfigEntry(inf_ctx.session_opt, "ort.ep.dml.enable_internal_graph_cache", "1");
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    status = inf_ctx.api->GetExecutionProviderApi("DML", ORT_API_VERSION, (const void **)&inf_ctx.dml_api);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    status = inf_ctx.dml_api->SessionOptionsAppendExecutionProvider_DML(inf_ctx.session_opt, inf_ctx.hardware_id);
    if (status) mscbl_log_warn("ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
}

void inference_create_session(Arena *arena)
{
#if OS_WIN32
    OrtStatus *status = inf_ctx.api->CreateSession(inf_ctx.env, WCStrCast(str8_to_wide(arena, inf_ctx.text_cfg.model_path)), inf_ctx.session_opt, &inf_ctx.text_sess);
#else
    OrtStatus *status = inf_ctx.api->CreateSession(inf_ctx.env, inf_ctx.text_cfg.model_path, inf_ctx.session_opt, &inf_ctx.text_sess);
#endif

#if OS_WIN32
    status = inf_ctx.api->CreateSession(inf_ctx.env, WCStrCast(str8_to_wide(arena, inf_ctx.vision_cfg.model_path)), inf_ctx.session_opt, &inf_ctx.vision_sess);
#else
    status = inf_ctx.api->CreateSession(inf_ctx.env, inf_ctx.vision_cfg.model_path, inf_ctx.session_opt, &inf_ctx.vision_sess);
#endif
}

ThreadFunc(inference_switch_hardware)
{
    ins_atomic_u32_eval_assign(&inf_ctx.state, InferenceState_Initializing);
    inf_ctx.api->SessionOptionsSetLoadCancellationFlag(inf_ctx.session_opt, 1);
    DeferLoop(os_mutex_lock(&inf_ctx.session_lock), os_mutex_unlock(&inf_ctx.session_lock))
    {
        // Teardown sessions and session options
        if (inf_ctx.text_sess)
        {
            inf_ctx.api->ReleaseSession(inf_ctx.text_sess);
            inf_ctx.text_sess = 0;
        }
        if (inf_ctx.vision_sess)
        {
            inf_ctx.api->ReleaseSession(inf_ctx.vision_sess);
            inf_ctx.vision_sess = 0;
        }
        if (inf_ctx.session_opt)
        {
            inf_ctx.api->ReleaseSessionOptions(inf_ctx.session_opt);
            inf_ctx.session_opt = 0;
        }

        // Switch device in session options
        inference_create_session_opt();

        // Re-create sessions with new session options
        inference_create_session(arena);
    }
    ins_atomic_u32_eval_assign(&inf_ctx.state, InferenceState_Ready);
}

InferenceState inference_state_get()
{
    return (InferenceState)ins_atomic_u32_eval(&inf_ctx.state);
}

VisionModelConfig *inference_preprocess_get()
{
    return &inf_ctx.vision_cfg;
}

InferenceHardwareArr inference_hardware_get()
{
    return inf_ctx.hardware;
}

static B32 check_token(jsmntok_t *token, Str8 key, Str8 content)
{
    Str8 token_str = str8_from_to(content, token->start, token->end);
    B32 match = str8_cmp(key, token_str);
    return (token->type == JSMN_STRING && key.size == token_str.size && match == 0);
}

Result inference_query_model(Str8 model_base)
{
    Result res = ResultSuccess();
    OrtAllocator *allocator = NULL;

    S64 dims[4];
    U64 ndims = 0;
    OrtTypeInfo *type_info = NULL;
    ONNXType onnx_type = ONNX_TYPE_UNKNOWN;
    const OrtTensorTypeAndShapeInfo *tensor_info = NULL;

    OrtStatusPtr status = inf_ctx.api->GetAllocatorWithDefaultOptions(&allocator);
    ORTResult(status);

    // Text model
    status = inf_ctx.api->SessionGetInputTypeInfo(inf_ctx.text_sess, 0, &type_info);
    ORTResult(status);
    status = inf_ctx.api->GetOnnxTypeFromTypeInfo(type_info, &onnx_type);
    ORTResult(status);
    status = inf_ctx.api->CastTypeInfoToTensorInfo(type_info, &tensor_info);
    ORTResult(status);
    status = inf_ctx.api->GetDimensionsCount(tensor_info, &ndims);
    ORTResult(status);
    status = inf_ctx.api->GetDimensions(tensor_info, dims, ndims);
    ORTResult(status);
    inf_ctx.text_cfg.token_length = dims[ndims - 1];
    inf_ctx.api->ReleaseTypeInfo(type_info);

    // Vision model
    status = inf_ctx.api->SessionGetInputTypeInfo(inf_ctx.vision_sess, 0, &type_info);
    ORTResult(status);
    status = inf_ctx.api->GetOnnxTypeFromTypeInfo(type_info, &onnx_type);
    ORTResult(status);
    status = inf_ctx.api->CastTypeInfoToTensorInfo(type_info, &tensor_info);
    ORTResult(status);
    status = inf_ctx.api->GetDimensionsCount(tensor_info, &ndims);
    ORTResult(status);
    status = inf_ctx.api->GetDimensions(tensor_info, dims, ndims);
    ORTResult(status);
    inf_ctx.vision_cfg.input_size = dims[ndims - 1];
    inf_ctx.api->ReleaseTypeInfo(type_info);

    status = 0;
    type_info = 0;

Cleanup:
    if (status)
        inf_ctx.api->ReleaseStatus(status);
    if (type_info)
        inf_ctx.api->ReleaseTypeInfo(type_info);

    return res;
}

Result inference_parse_config(Arena *arena, Str8 model_base)
{
    Temp scratch = temp_begin(arena);
    Result res = ResultSuccess();

    // Finding input sizes
    StringBuilder config_path = str8_init(arena, model_base);
    str8_path_join(&config_path, sv8_lit("preprocessor_config.json"));

    jsmn_parser parser;
    jsmn_init(&parser);
    jsmntok_t tokens[KB(1)];

    FileHandle config_handle = 0;
    U64 content_size = 0;
    StringBuilder config_content = {0};

    B32 exist = os_path_exists(Str8Cast(config_path), &res);
    CheckAndClearResult(res);
    res = GenResult(exist, Domain_App, AppError_FileNotFound, "config file not found");
    CheckAndClearResult(res);

    config_handle = os_file_open(Str8Cast(config_path), FileAccess_Read, FileMode_OpenAlways, &res);
    CheckAndClearResult(res);
    content_size = os_file_size(config_handle, &res);
    CheckAndClearResult(res);
    config_content = str8_empty(arena, content_size);
    os_file_read(config_handle, content_size, config_content.v, &res);
    CheckAndClearResult(res);
    config_content.size = content_size;

    {
        U32 num_tokens = jsmn_parse(&parser, CStrCast(config_content), content_size, tokens, StaticArrSize(tokens));
        res = GenResult(num_tokens > 0, Domain_JSON, 0, "no json token found");

        for (U32 i = 0; i < num_tokens; i++)
        {
            char buf[KB(1)];
            jsmntok_t key_tok = tokens[i];

            if (check_token(&key_tok, sv8_lit("image_mean"), Str8Cast(config_content)))
            {
                jsmntok_t val_tok = tokens[i + 1];
                if (val_tok.type == JSMN_ARRAY)
                {
                    jsmntok_t *val0 = &tokens[i + 1 + 1];
                    inf_ctx.vision_cfg.mean[0] = str8_to_f32(str8_from_to(Str8Cast(config_content), val0->start, val0->end));
                    jsmntok_t *val1 = &tokens[i + 1 + 2];
                    inf_ctx.vision_cfg.mean[1] = str8_to_f32(str8_from_to(Str8Cast(config_content), val1->start, val1->end));
                    jsmntok_t *val2 = &tokens[i + 1 + 3];
                    inf_ctx.vision_cfg.mean[2] = str8_to_f32(str8_from_to(Str8Cast(config_content), val2->start, val2->end));
                    i += 4;
                    continue;
                }
            }
            if (check_token(&key_tok, sv8_lit("image_std"), Str8Cast(config_content)))
            {
                jsmntok_t val_tok = tokens[i + 1];
                if (val_tok.type == JSMN_ARRAY)
                {
                    jsmntok_t *val0 = &tokens[i + 1 + 1];
                    inf_ctx.vision_cfg.std_dev[0] = str8_to_f32(str8_from_to(Str8Cast(config_content), val0->start, val0->end));
                    jsmntok_t *val1 = &tokens[i + 1 + 2];
                    inf_ctx.vision_cfg.std_dev[1] = str8_to_f32(str8_from_to(Str8Cast(config_content), val1->start, val1->end));
                    jsmntok_t *val2 = &tokens[i + 1 + 3];
                    inf_ctx.vision_cfg.std_dev[2] = str8_to_f32(str8_from_to(Str8Cast(config_content), val2->start, val2->end));
                    i += 4;
                    continue;
                }
            }
            if (check_token(&key_tok, sv8_lit("rescale_factor"), Str8Cast(config_content)))
            {
                jsmntok_t val_tok = tokens[i + 1];
                U64 tok_len = val_tok.end - val_tok.start;

                inf_ctx.vision_cfg.rescale_factor = str8_to_f64(str8_from_to(Str8Cast(config_content), val_tok.start, val_tok.end));
                i++;
                continue;
            }
        }
    }

Cleanup:
    os_file_close(config_handle, &res);

    temp_end(scratch);
    return res;
}

void ORT_API_CALL ort_logger(void *param, OrtLoggingLevel severity, const char *category, const char *logid, const char *code_location, const char *message)
{
    const char *level = NULL;
    switch (severity)
    {
    case ORT_LOGGING_LEVEL_VERBOSE:
    case ORT_LOGGING_LEVEL_INFO:
        level = "INFO";
        break;
    case ORT_LOGGING_LEVEL_WARNING:
        level = "WARN";
        break;
    case ORT_LOGGING_LEVEL_ERROR:
    case ORT_LOGGING_LEVEL_FATAL:
        level = "ERROR";
        break;
    }

    Str8 message_str = sv8_lit(message);
    while (message_str.v[message_str.size - 1] == '\n')
        message_str.size--;
    mscbl_log_bare(level, code_location, "[%s] %.*s", category, Str8Spr(message_str));
}

Embedding inference_text_embedding(Arena *arena, Str8 input)
{
    if (inference_state_get() != InferenceState_Ready)
    {
        ui_push_message(GenResult(inference_state_get() == InferenceState_Ready, Domain_App, AppError_PreconditionFail, "Inference not ready"));
        return {.vector = 0};
    }

    if (!input.size)
    {
        ui_push_message(GenResult(inference_state_get() != InferenceState_Ready, Domain_App, AppError_InvalidInput, "Empty input"));
        return {.vector = 0};
    }

    extError_t ortx_err = kOrtxOK;

    U64 token_array_length = 0;
    const extTokenId_t *token_ids = NULL;
    OrtxTokenId2DArray *_token_array = NULL;

    const char *tokens[] = {CStrCast(input)};
    os_mutex_lock(&inf_ctx.text_cfg.tokenizer_lock);
    ortx_err = OrtxTokenize(inf_ctx.text_cfg.tokenizer, tokens, 1, &_token_array);
    os_mutex_unlock(&inf_ctx.text_cfg.tokenizer_lock);
    Assert(ortx_err == kOrtxOK, "ORTX error: %s", OrtxGetLastErrorMessage());
    ortx_err = OrtxTokenId2DArrayGetItem(_token_array, 0, &token_ids, &token_array_length);
    Assert(ortx_err == kOrtxOK, "ORTX error: %s", OrtxGetLastErrorMessage());

    S64 *input_data = push_array0(arena, inf_ctx.text_cfg.token_length, S64);
    S64 *mask_data = push_array0(arena, inf_ctx.text_cfg.token_length, S64);

    for (U64 i = 0; i < MIN(token_array_length, inf_ctx.text_cfg.token_length); i++)
    {
        input_data[i] = token_ids[i];
        mask_data[i] = 1;
    }

    ORTX_DISPOSE(_token_array);

    const char *input_names[] = {"input", "mask"};
    const char *output_names[] = {"embedding"};

    OrtValue *text_tensor = NULL;
    OrtValue *mask_tensor = NULL;
    OrtValue *output_tensor = NULL;
    OrtMemoryInfo *memory_info = NULL;

    OrtStatus *status = NULL;

    status = inf_ctx.api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    U64 data_size = inf_ctx.text_cfg.token_length * sizeof(S64);
    S64 data_shape[2] = {1, inf_ctx.text_cfg.token_length};
    status = inf_ctx.api->CreateTensorWithDataAsOrtValue(memory_info, input_data, data_size, data_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &text_tensor);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
    status = inf_ctx.api->CreateTensorWithDataAsOrtValue(memory_info, mask_data, data_size, data_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &mask_tensor);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    OrtValue *input_tensors[2] = {text_tensor, mask_tensor};
    os_mutex_lock(&inf_ctx.session_lock);
    status = inf_ctx.api->Run(inf_ctx.text_sess, NULL, input_names, input_tensors, 2, output_names, 1, &output_tensor);
    os_mutex_unlock(&inf_ctx.session_lock);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    S64 out_dims[2];
    U64 out_ndim = 0;
    F32 *output_data = NULL;
    OrtTensorTypeAndShapeInfo *shape_info = NULL;
    status = inf_ctx.api->GetTensorTypeAndShape(output_tensor, &shape_info);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
    status = inf_ctx.api->GetDimensionsCount(shape_info, &out_ndim);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
    status = inf_ctx.api->GetDimensions(shape_info, out_dims, out_ndim);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    status = inf_ctx.api->GetTensorMutableData(output_tensor, (void **)&output_data);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    inf_ctx.api->ReleaseTensorTypeAndShapeInfo(shape_info);

    U64 output_element_count = out_dims[0] * out_dims[1];
    Embedding output = {.dimension = (U32)out_dims[out_ndim - 1], .batch_size = 1};
    output.vector = push_array(arena, output_element_count, F32);
    MemoryCopy(output.vector, output_data, output_element_count * sizeof(F32));

    ops_embedding_normalize(output);

    inf_ctx.api->ReleaseValue(text_tensor);
    inf_ctx.api->ReleaseValue(mask_tensor);
    inf_ctx.api->ReleaseValue(output_tensor);
    inf_ctx.api->ReleaseMemoryInfo(memory_info);

    return output;
}

Embedding inference_vision_embedding(Arena *arena, F32 *data, U32 batch_size)
{
    const char *input_names[] = {"input"};
    const char *output_names[] = {"embedding"};

    OrtValue *vision_tensor = NULL;
    OrtValue *output_tensor = NULL;
    OrtMemoryInfo *memory_info = NULL;

    OrtStatus *status = NULL;

    status = inf_ctx.api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    S64 input_shape[4] = {batch_size, 3, inf_ctx.vision_cfg.input_size, inf_ctx.vision_cfg.input_size};
    U64 data_size = batch_size * 3 * inf_ctx.vision_cfg.input_size * inf_ctx.vision_cfg.input_size * sizeof(F32);
    status = inf_ctx.api->CreateTensorWithDataAsOrtValue(memory_info, data, data_size, input_shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &vision_tensor);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    os_mutex_lock(&inf_ctx.session_lock);
    status = inf_ctx.api->Run(inf_ctx.vision_sess, NULL, input_names, &vision_tensor, 1, output_names, 1, &output_tensor);
    os_mutex_unlock(&inf_ctx.session_lock);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    S64 out_dims[2];
    U64 out_ndim = 0;
    F32 *output_data = NULL;
    OrtTensorTypeAndShapeInfo *shape_info = NULL;
    status = inf_ctx.api->GetTensorTypeAndShape(output_tensor, &shape_info);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
    status = inf_ctx.api->GetDimensionsCount(shape_info, &out_ndim);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
    status = inf_ctx.api->GetDimensions(shape_info, out_dims, out_ndim);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    status = inf_ctx.api->GetTensorMutableData(output_tensor, (void **)&output_data);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));

    inf_ctx.api->ReleaseTensorTypeAndShapeInfo(shape_info);

    U64 output_element_count = out_dims[0] * out_dims[1];
    Embedding output = {.dimension = (U32)out_dims[out_ndim - 1], .batch_size = batch_size};
    output.vector = push_array(arena, output_element_count, F32);
    MemoryCopy(output.vector, output_data, output_element_count * sizeof(F32));

    ops_embedding_normalize(output);

    inf_ctx.api->ReleaseValue(vision_tensor);
    inf_ctx.api->ReleaseValue(output_tensor);
    inf_ctx.api->ReleaseMemoryInfo(memory_info);

    return output;
}

void inference_fetch_hardware(Arena *arena)
{
    U64 device_count = 0;
    const OrtEpDevice *const *ep_devices = NULL;
    OrtStatus *status = inf_ctx.api->GetEpDevices(inf_ctx.env, &ep_devices, &device_count);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
    da_setcap(arena, inf_ctx.hardware, device_count);

    for (U64 i = 0; i < device_count; i++)
    {
        const OrtEpDevice *device = ep_devices[i];
        if (device != NULL)
        {
            const OrtHardwareDevice *dvc = inf_ctx.api->EpDevice_Device(device);

            U64 count = 0;
            const char *const *keys = NULL;
            const char *const *vals = NULL;
            inf_ctx.api->GetKeyValuePairs(inf_ctx.api->HardwareDevice_Metadata(dvc), &keys, &vals, &count);

            InferenceHardware hardware_info = {
                .hw_desc = str8_copy(arena, vals[0]),
                .hw_vendor = str8_copy(arena, inf_ctx.api->HardwareDevice_Vendor(dvc)),
                .hw_type = inf_ctx.api->HardwareDevice_Type(dvc),
            };
            da_push(arena, inf_ctx.hardware, hardware_info);
        }
    }
}

ThreadFunc(inference_backend_init)
{
    ins_atomic_u32_eval_assign(&inf_ctx.state, InferenceState_Initializing);

    OrtStatus *status = inf_ctx.api->CreateEnvWithCustomLogger(ort_logger, NULL, ORT_LOGGING_LEVEL_WARNING, "ORT", &inf_ctx.env);
    Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
#if OS_WIN32
    // fetch available hardware list before setting one
    inference_fetch_hardware(inf_ctx.arena);
    inf_ctx.hardware_id = mscbl_config.inf_settings.dml_device;
    if (inf_ctx.hardware_id >= arr_getsize(inf_ctx.hardware))
    {
        inf_ctx.hardware_id = 0;
        mscbl_log_warn("Preferred inference hardware not available. Using default %.*s", Str8Spr(inf_ctx.hardware[0].hw_desc));
    }

    inference_create_session_opt();
#else
    const char *ov_keys[] = {"device_type", "precision"};
    const char *ov_vals[] = {"AUTO", "FP16"};
    status = inf_ctx.api->SessionOptionsAppendExecutionProvider_OpenVINO_V2(inf_ctx.session_opt, ov_keys, ov_vals, StaticArrSize(ov_keys));
    if (status)
        mscbl_log_warn("ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
#endif

    // U64 device_count = 0;
    // const OrtEpDevice *const *ep_devices = NULL;
    // status = inf_ctx.api->GetEpDevices(inf_ctx.env, &ep_devices, &device_count);
    // Assert(!status, "ORT API Error: %.*s", Str8Spr(ORTErrorMessage(inf_ctx.api, status)));
    //
    // for (U64 i = 0; i < device_count; i++)
    // {
    //     const OrtEpDevice *device = ep_devices[i];
    //     if (device != NULL)
    //     {
    //         mscbl_log_info("Device: %s - %s", inf_ctx.api->EpDevice_EpVendor(device), inf_ctx.api->EpDevice_EpName(device));
    //         const OrtHardwareDevice *dvc = inf_ctx.api->EpDevice_Device(device);
    //
    //         OrtHardwareDeviceType dvc_type = inf_ctx.api->HardwareDevice_Type(dvc);
    //         U32 dvc_vid = inf_ctx.api->HardwareDevice_VendorId(dvc);
    //         const char *dvc_vnd = inf_ctx.api->HardwareDevice_Vendor(dvc);
    //         U32 dvc_hid = inf_ctx.api->HardwareDevice_DeviceId(dvc);
    //
    //         mscbl_log_info("Info:");
    //         switch (dvc_type)
    //         {
    //         case OrtHardwareDeviceType_CPU: mscbl_log_info("\tType: CPU"); break;
    //         case OrtHardwareDeviceType_GPU: mscbl_log_info("\tType: GPU"); break;
    //         case OrtHardwareDeviceType_NPU: mscbl_log_info("\tType: NPU"); break;
    //         }
    //         mscbl_log_info("\tVID: %u", dvc_vid);
    //         mscbl_log_info("\tVendor: %s", dvc_vnd);
    //         mscbl_log_info("\tHID: %u", dvc_hid);
    //
    //         const OrtKeyValuePairs *dvc_kv = inf_ctx.api->HardwareDevice_Metadata(dvc);
    //         if (dvc_kv)
    //         {
    //             const char *const *keys = NULL;
    //             const char *const *vals = NULL;
    //             U64 size = 0;
    //
    //             inf_ctx.api->GetKeyValuePairs(dvc_kv, &keys, &vals, &size);
    //
    //             mscbl_log_info("\tMetadata:");
    //             for (U64 k = 0; k < size; k++)
    //             {
    //                 mscbl_log_info("\t\t[%s] => %s", keys[k] ? keys[k] : "_", vals[k] ? vals[k] : "_");
    //             }
    //         }
    //     }
    // }

    ActiveModel active_model = mscbl_config.inf_settings.active;

    StringBuilder model_base = str8_init(arena, mscbl_config.inf_settings.base_dir);
    str8_path_join(&model_base, active_model.group->name);
    os_mkdirs(Str8Cast(model_base));

    StringBuilder text_filepath = str8_init(app_arena, Str8Cast(model_base));
    str8_path_join(&text_filepath, active_model.variant->text[0].name);
    inf_ctx.text_cfg.model_path = Str8Cast(text_filepath);

    StringBuilder vision_filepath = str8_init(app_arena, Str8Cast(model_base));
    str8_path_join(&vision_filepath, active_model.variant->vision[0].name);
    inf_ctx.vision_cfg.model_path = Str8Cast(vision_filepath);

    StringBuilder checkpoint_path = str8_init(arena, mscbl_config.inf_settings.base_dir);
    str8_path_join(&checkpoint_path, sv8_lit(".checkpoint"));
    FileHandle checkpoint_handle = 0;

    Result res = ResultSuccess();

    res = inference_tokenizer_init(Str8Cast(model_base));
    CheckAndClearResult(res);

    if (!os_path_exists(Str8Cast(checkpoint_path), &res))
    {
        res = model_download_files(arena, active_model.group->common_files, active_model.group->common_files_size, Str8Cast(model_base));
        CheckAndClearResult(res);
        res = model_download_files(arena, active_model.variant->text, active_model.variant->text_size, Str8Cast(model_base));
        CheckAndClearResult(res);
        res = model_download_files(arena, active_model.variant->vision, active_model.variant->vision_size, Str8Cast(model_base));
        CheckAndClearResult(res);

        checkpoint_handle = os_file_open(Str8Cast(checkpoint_path), FileAccess_Write, FileMode_CreateAlways, &res);
        CheckAndClearResult(res);
        os_file_close(checkpoint_handle, &res);
        CheckAndClearResult(res);
    }

    res = GenResult(active_model.backend == Backend_ONNX, Domain_App, AppError_Unimplemented, "GGML not available");
    CheckAndClearResult(res);

    os_mutex_lock(&inf_ctx.session_lock);
#if OS_WIN32
    status = inf_ctx.api->CreateSession(inf_ctx.env, WCStrCast(str8_to_str16(arena, inf_ctx.text_cfg.model_path)), inf_ctx.session_opt, &inf_ctx.text_sess);
#else
    status = inf_ctx.api->CreateSession(inf_ctx.env, inf_ctx.text_cfg.model_path)), inf_ctx.session_opt, &inf_ctx.text_sess);
#endif
    os_mutex_unlock(&inf_ctx.session_lock);
    res = GenResult(!status, Domain_Inference_ONNX, inf_ctx.api->GetErrorCode(status), inference_ort_err(inf_ctx.api->GetErrorCode(status)));
    CheckAndClearResult(res);

    os_mutex_lock(&inf_ctx.session_lock);
#if OS_WIN32
    status = inf_ctx.api->CreateSession(inf_ctx.env, WCStrCast(str8_to_str16(arena, inf_ctx.vision_cfg.model_path)), inf_ctx.session_opt, &inf_ctx.vision_sess);
#else
    status = inf_ctx.api->CreateSession(inf_ctx.env, inf_ctx.vision_cfg.model_path, inf_ctx.session_opt, &inf_ctx.vision_sess);
#endif
    os_mutex_unlock(&inf_ctx.session_lock);
    res = GenResult(!status, Domain_Inference_ONNX, inf_ctx.api->GetErrorCode(status), inference_ort_err(inf_ctx.api->GetErrorCode(status)));
    CheckAndClearResult(res);

    // res = inference_query_model(Str8Cast(model_base));
    // CheckAndClearResult(res);
    // res = inference_parse_config(arena, Str8Cast(model_base));
    // CheckAndClearResult(res);

    ins_atomic_u32_eval_assign(&inf_ctx.state, InferenceState_Ready);
    threadpool_enqueue(TaskPriority_Low, {.func = model_insert_embedding});
    return;

Cleanup:
    ins_atomic_u32_eval_assign(&inf_ctx.state, InferenceState_Failed);
    if (status) inf_ctx.api->ReleaseStatus(status);
    ui_push_message(res);
    os_file_close(checkpoint_handle, &res);
}
