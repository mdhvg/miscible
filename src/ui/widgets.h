#pragma once
#include "db/view.h"
#include "base/log.h"
#include "base/base_core.h"

global_v const char *byte_string(ByteUnit unit);
global_v const char *month_string(Month month);

MSCBL_API void input_bytesize(ByteSize *source);
MSCBL_API void input_date(Date *source);
