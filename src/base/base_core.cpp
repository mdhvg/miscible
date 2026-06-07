#include "base/base_core.h"

global_v const char hex_table_upper[] = "0123456789ABCDEF";
global_v const char hex_table_lower[] = "0123456789abcdef";

void bytes_as_hex_lower(U8 *data, U64 start, U64 len, char *out)
{
    for (U64 i = start; i < len; i++)
    {
        out[i * 2] = hex_table_lower[data[i] >> 4];
        out[i * 2 + 1] = hex_table_lower[data[i] & 0x0F];
    }
}

void bytes_as_hex_upper(U8 *data, U64 start, U64 len, char *out)
{
    for (U64 i = start; i < len; i++)
    {
        out[i * 2] = hex_table_upper[data[i] >> 4];
        out[i * 2 + 1] = hex_table_upper[data[i] & 0x0F];
    }
}

U64 time_to_timestamp(Time time)
{
    // NOTE: it's only a calculation of days since 1 Jan 1970 (UNIX epoch)
    U64 days_since = (time.year - 1970) * 365;
    U64 leap_years = ((time.year - 1) / 4) - ((time.year - 1) / 100) + ((time.year - 1) / 400) - 477;

    days_since += leap_years;

    for (S32 mon = Month_Jan; mon < time.month; mon++)
    {
        switch (mon)
        {
        case Month_Jan:
        case Month_Mar:
        case Month_May:
        case Month_Jul:
        case Month_Aug:
        case Month_Oct:
        case Month_Dec:
            days_since += 31;
            break;

        case Month_Apr:
        case Month_Jun:
        case Month_Sep:
        case Month_Nov:
            days_since += 30;
            break;

        case Month_Feb:
            if (time.year % 4 == 0 && (time.year % 100 != 0 || time.year % 400 == 0))
                days_since += 29;
            else
                days_since += 28;
            break;
        }
    }

    // NOTE: -1 means first second of the day
    days_since += time.date - 1;

    return days_since * 86400;
}

Time timestamp_to_time(U64 timestamp)
{
    // NOTE: it's only a calculation of days since 1 Jan 1970 (UNIX epoch)
    U64 days_size = timestamp / 86400;

    Time time = {.date = 1, .month = Month_Jan, .year = 1970};

    while (1)
    {
        U32 days_in_year = (time.year % 4 == 0 && (time.year % 100 != 0 || time.year % 400 == 0)) ? 366 : 365;
        if (days_size < days_in_year)
            break;

        days_size -= days_in_year;
        time.year++;
    }

    for (U32 mon = Month_Jan; mon <= Month_Dec; mon++)
    {
        U32 days_in_mon = 0;
        switch (mon)
        {
        case Month_Jan:
        case Month_Mar:
        case Month_May:
        case Month_Jul:
        case Month_Aug:
        case Month_Oct:
        case Month_Dec:
            days_in_mon = 31;
            break;
        case Month_Apr:
        case Month_Jun:
        case Month_Sep:
        case Month_Nov:
            days_in_mon = 30;
            break;
        case Month_Feb:
            days_in_mon = (time.year % 4 == 0 && (time.year % 100 != 0 || time.year % 400 == 0)) ? 29 : 28;
            break;
        }

        if (days_size < days_in_mon)
        {
            time.month = (Month)mon;
            break;
        }
        days_size -= days_in_mon;
    }

    // NOTE: +1 adjust for 0 indexing
    time.date = days_size + 1;

    return time;
}

U32 month_days(Time time)
{
    switch (time.month)
    {
    case Month_Jan:
    case Month_Mar:
    case Month_May:
    case Month_Jul:
    case Month_Aug:
    case Month_Oct:
    case Month_Dec:
        return 31;

    case Month_Apr:
    case Month_Jun:
    case Month_Sep:
    case Month_Nov:
        return 30;

    case Month_Feb:
        if (time.year % 4 == 0 && (time.year % 100 != 0 || time.year % 400 == 0))
            return 29;
        else
            return 28;

    default: return 0;
    }
}

ByteSize size_to_bytesize(U64 size)
{
    F64 value = (F64)size;
    U32 units = Byte;

    while (value >= 1024.0f && units < Byte_COUNT)
    {
        value /= 1024.0f;
        units++;
    }

    return {(F32)value, (ByteUnit)units};
}
