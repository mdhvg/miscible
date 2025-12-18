#include "base/base_core.h"

static const char hex_table_upper[] = "0123456789ABCDEF";
static const char hex_table_lower[] = "0123456789abcdef";

void bytes_as_hex_lower(U8 *data, U64 start, U64 len, char *out)
{
	for (U64 i = start; i < len; i++)
	{
		out[i * 2]	   = hex_table_lower[data[i] >> 4];
		out[i * 2 + 1] = hex_table_lower[data[i] & 0x0F];
	}
}

void bytes_as_hex_upper(U8 *data, U64 start, U64 len, char *out)
{
	for (U64 i = start; i < len; i++)
	{
		out[i * 2]	   = hex_table_upper[data[i] >> 4];
		out[i * 2 + 1] = hex_table_upper[data[i] & 0x0F];
	}
}