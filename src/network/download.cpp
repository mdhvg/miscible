// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "sha2.h"
#include "curl/curl.h"

#include "base/log.h"
#include "ui/ui_core.h"
#include "base/arena.h"
#include "base/string.h"
#include "app/miscible.h"
#include "base/threadpool.h"

#include "network/download.h"

#define DOWNLOAD_RETRIES     10
#define DOWNLOAD_PIECE_SIZE  MB(4)
#define LARGE_FILE_THRESHOLD MB(10)

struct download_params
{
    U8 *state;
    S64 index;
    U64 file_size;
    StringArr file_urls;
    FileHandle file_desc;
};

struct cbk_params
{
    U8 *buffer;
    U64 used;
};

static U64 largefile_cbk(U8 *data, U64 n, U64 l, void *userp)
{
    cbk_params *params = (cbk_params *)userp;

    U8 *bytes_p = params->buffer;
    U64 used = params->used;

    MemoryCopy(bytes_p + used, data, n * l);
    params->used += n * l;

    return n * l;
}

ThreadFunc(download_worker)
{
    arena_clear(arena);
    Assert(args[0].kind == TPData_Any, "wrong datatype");
    Assert(args[1].kind == TPData_S64, "wrong datatype");
    Assert(args[2].kind == TPData_U64, "wrong datatype");
    Assert(args[3].kind == TPData_String, "wrong datatype");
    Assert(args[4].kind == TPData_FileDesc, "wrong datatype");

    U8 *state_array = (U8 *)args[0].val_any;
    S64 index = args[1].val_s64;
    U64 file_size = args[2].val_u64;
    String file_url = args[3].val_str;
    FileHandle tempfile_desc = args[4].val_filedesc;

    U64 start = index * DOWNLOAD_PIECE_SIZE;
    U64 end = MIN(start + DOWNLOAD_PIECE_SIZE - 1, file_size - 1);
    U64 size = end - start + 1;

    char range[256] = {0};
    snprintf(range, 256, "%zu-%zu", start, end);
    U8 *array = push_array(arena, size, U8);
    cbk_params params1 = {0};

    CURLcode curl_err = CURLE_OK;
    Result res = ResultSuccess();

    perf_beg(download);
    ArenaScoped(arena)
    {
        CURL *curl = NULL;
        curl = curl_easy_init();
        if (!curl)
            continue;

        for (U32 i = 0; i < DOWNLOAD_RETRIES; i++)
        {
            curl_err = curl_easy_setopt(curl, CURLOPT_URL, CStrCast(file_url));
            if (curl_err != CURLE_OK) goto CURLError;

            curl_err = curl_easy_setopt(curl, CURLOPT_RANGE, range);
            if (curl_err != CURLE_OK) goto CURLError;

            curl_err = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            if (curl_err != CURLE_OK) goto CURLError;

            params1 = {.buffer = array, .used = 0};
            curl_err = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &params1);
            if (curl_err != CURLE_OK) goto CURLError;

            curl_err = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, largefile_cbk);
            if (curl_err != CURLE_OK) goto CURLError;

            curl_err = curl_easy_perform(curl);
            if (curl_err != CURLE_OK) goto CURLError;

            if (params1.used != size)
            {
                res = {.success = 0, .domain = Domain_App, .code = AppError_PartialDownload};
                goto Cleanup;
            }

            os_file_write(tempfile_desc, size, params1.buffer, &res, start);
            CheckAndClearResult(res);

            BitFieldSet(state_array, index);
            break; // Success

        CURLError:
            res = {
                .success = 0,
                .domain = Domain_Network,
                .code = (U32)curl_err,
                .context = curl_easy_strerror(curl_err)};
        Cleanup:
            ui_push_message(res);
            continue;
        }

        if (curl) curl_easy_cleanup(curl);
    }
    perf_end(download);
}

Result download_large_file(Arena *arena, DownloadArgs args)
{
    Result res = ResultSuccess();
    Result cleanup_res = ResultSuccess();

    S64 block_count = ToCeilInt(args.file_size, DOWNLOAD_PIECE_SIZE);
    U64 state_array_size = ToCeilInt(block_count, (sizeof(U8) * 8));

    StringBuilder statefile_path = string_init(arena, StringCast(args.filepath));
    string_push(&statefile_path, ".state");

    StringBuilder tempfile_path = string_init(arena, StringCast(args.filepath));
    string_push(&tempfile_path, ".temp");

    sha256_ctx ctx = {0};
    U8 *file_buffer = NULL;
    U8 file_hash[SHA256_DIGEST_SIZE] = {0};

    FileHandle statefile_desc = 0;
    FileHandle tempfile_desc = 0;

    U8 *state_array = NULL;
    B32 download_finish = 1;
    S64 batch_size = block_count;

    OSMmap map = {0};
    statefile_desc = os_file_open(StringCast(statefile_path), FileAccess_Read | FileAccess_Write, FileMode_OpenAlways, &res);
    CheckAndClearResult(res);

    map = os_file_map(statefile_desc, state_array_size, &res);
    CheckAndClearResult(res);
    state_array = (U8 *)os_map_get_data(map);

    tempfile_desc = os_file_open(StringCast(tempfile_path), FileAccess_Read | FileAccess_Write, FileMode_OpenAlways, &res);
    CheckAndClearResult(res);

    for (S64 i = 0; i < block_count; i++)
    {
        if (BitFieldGet(state_array, i))
            batch_size--;
    }

    if (batch_size)
    {
        Semaphore wait = os_semaphore_init(0, S32_MAX);
        for (S64 block = 0; block < block_count; block++)
        {
            if (BitFieldGet(state_array, block))
                continue;

            AsyncTask task = {
                .func = download_worker,
                .args = {
                    {.kind = TPData_Any, .val_any = state_array},
                    {.kind = TPData_S64, .val_s64 = block},
                    {.kind = TPData_U64, .val_u64 = args.file_size},
                    {.kind = TPData_String, .val_str = args.link},
                    {.kind = TPData_FileDesc, .val_filedesc = tempfile_desc},
                },
                .batch_size = &batch_size,
                .batch_complete = wait};
            threadpool_enqueue(TaskPriority_Low, task);
        }

        os_semaphore_pop(wait, U64_MAX);
        os_semaphore_destroy(wait);
    }

    for (S64 i = 0; i < block_count; i++)
    {
        download_finish &= BitFieldGet(state_array, i);
    }
    if (!download_finish)
    {
        res = {
            .success = 0,
            .domain = Domain_App,
            .code = AppError_PartialDownload,
            .context = "Download finished but data blocks are incomplete"};
        goto Cleanup;
    }

    os_file_unmap(map, &res);
    CheckAndClearResult(res);
    map = {0};

    os_file_close(statefile_desc, &res);
    CheckAndClearResult(res);
    statefile_desc = 0;

    sha256_init(&ctx);
    for (S64 bi = 0; bi < block_count; bi++)
    {
        ArenaScoped(arena)
        {
            U64 start = bi * DOWNLOAD_PIECE_SIZE;
            U64 end = MIN(start + DOWNLOAD_PIECE_SIZE, args.file_size);
            U64 size = end - start;

            file_buffer = push_array(arena, size, U8);
            os_file_read(tempfile_desc, size, file_buffer, &res, start);

            sha256_update(&ctx, file_buffer, size);
        }
    }
    sha256_final(&ctx, file_hash);
    if (memcmp(file_hash, args.file_hash, SHA256_DIGEST_SIZE) != 0)
    {
        os_file_delete(StringCast(statefile_path), &res);
        res = {.success = 0,
               .domain = Domain_App,
               .code = AppError_ChecksumFail,
               .context = "Manifest checksum verification failed (corrupt download)"};
        goto Cleanup;
    }

    os_file_close(tempfile_desc, &res);
    CheckAndClearResult(res);
    tempfile_desc = 0;

    os_file_rename(StringCast(tempfile_path), StringCast(args.filepath));

    goto Return;

Cleanup:
    if (map.handle)
    {
        os_file_unmap(map, &cleanup_res);
        if (!cleanup_res.success)
            res = cleanup_res;
    }

    os_file_close(statefile_desc, &cleanup_res);
    if (!cleanup_res.success)
        res = cleanup_res;

    os_file_close(tempfile_desc, &cleanup_res);
    if (!cleanup_res.success)
        res = cleanup_res;

Return:
    return res;
}

static U64 smallfile_cbk(U8 *data, U64 n, U64 l, void *userp)
{
    StringBuilder *base = (StringBuilder *)userp;
    String view = {(U8 *)data, n * l};
    string_push(base, view);
    return n * l;
}

Result download_small_file(Arena *arena, DownloadArgs args)
{
    Result res = ResultSuccess();
    Result cleanup_res = ResultSuccess();

    CURL *curl = NULL;
    FileHandle file = 0;
    CURLcode curl_err = CURLE_OK;

    sha256_ctx ctx = {0};
    U8 hash_found[SHA256_DIGEST_SIZE] = {0};

    StringBuilder file_content = string_empty(arena, args.file_size);

    curl = curl_easy_init();
    if (!curl)
    {
        res = {
            .success = 0,
            .domain = Domain_Network,
            .code = 0,
            .context = "curl_easy_init fail"};
        goto Cleanup;
    }

    // for (U32 li = 0; li < da_getsize(args.link); li++)
    // {
    // String url = args.link[li];

    curl_err = curl_easy_setopt(curl, CURLOPT_URL, CStrCast(args.link));
    if (curl_err != CURLE_OK) goto CurlError;

    curl_err = curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    if (curl_err != CURLE_OK) goto CurlError;

    curl_err = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    if (curl_err != CURLE_OK) goto CurlError;

    curl_err = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (curl_err != CURLE_OK) goto CurlError;

    curl_err = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file_content);
    if (curl_err != CURLE_OK) goto CurlError;

    curl_err = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, smallfile_cbk);
    if (curl_err != CURLE_OK) goto CurlError;

    curl_err = curl_easy_perform(curl);
    if (curl_err != CURLE_OK) goto CurlError;

    sha256_init(&ctx);
    sha256_update(&ctx, file_content.v, file_content.size);
    sha256_final(&ctx, hash_found);

    if (memcmp(hash_found, args.file_hash, SHA256_DIGEST_SIZE) != 0)
    {
        res = {.success = 0,
               .domain = Domain_App,
               .code = AppError_ChecksumFail,
               .context = "Manifest checksum verification failed (corrupt download)"};
        goto Cleanup;
    }
    // }

    file = os_file_open(args.filepath, FileAccess_Write, FileMode_OpenAlways, &res);
    CheckAndClearResult(res);
    os_file_write(file, StringSpr(file_content), &res);
    CheckAndClearResult(res);
    os_file_close(file, &res);
    CheckAndClearResult(res);

    goto Return;

CurlError:
    res = {
        .success = 0,
        .domain = Domain_Network,
        .code = (U32)curl_err,
        .context = curl_easy_strerror(curl_err)};
Cleanup:
    os_file_close(file, &cleanup_res);
    if (!cleanup_res.success)
        res = cleanup_res;
Return:
    if (curl)
        curl_easy_cleanup(curl);

    return res;
}

Result download_file(Arena *arena, DownloadArgs args)
{
    if (args.file_size >= LARGE_FILE_THRESHOLD)
    {
        return download_large_file(arena, args);
    }
    return download_small_file(arena, args);
}
