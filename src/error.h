#pragma once

enum AppError
{
    AppError_None = 0,
    AppError_NullPtr,
    AppError_ChecksumFail,
    AppError_PartialDownload,
};
