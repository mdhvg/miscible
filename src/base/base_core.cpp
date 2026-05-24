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

U64 date_to_timestamp(Date date)
{
    // NOTE: it's only a calculation of days since 1 Jan 1970 (UNIX epoch)
    U64 days_since = (date.year - 1970) * 365;
    U64 leap_years = ((date.year - 1) / 4) - ((date.year - 1) / 100) + ((date.year - 1) / 400) - 477;

    days_since += leap_years;

    for (S32 mon = Month_Jan; mon < date.month; mon++)
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
            if (date.year % 4 == 0 && (date.year % 100 != 0 || date.year % 400 == 0))
                days_since += 29;
            else
                days_since += 28;
            break;
        }
    }

    // NOTE: -1 means first second of the day
    days_since += date.date - 1;

    return days_since * 86400;
}

Date timestamp_to_date(U64 timestamp)
{
    // NOTE: it's only a calculation of days since 1 Jan 1970 (UNIX epoch)
    U64 days_size = timestamp / 86400;

    Date date = {.date = 1, .month = Month_Jan, .year = 1970};

    while (1)
    {
        U32 days_in_year = (date.year % 4 == 0 && (date.year % 100 != 0 || date.year % 400 == 0)) ? 366 : 365;
        if (days_size < days_in_year)
            break;

        days_size -= days_in_year;
        date.year++;
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
            days_in_mon = (date.year % 4 == 0 && (date.year % 100 != 0 || date.year % 400 == 0)) ? 29 : 28;
            break;
        }

        if (days_size < days_in_mon)
        {
            date.month = (Month)mon;
            break;
        }
        days_size -= days_in_mon;
    }

    // NOTE: +1 adjust for 0 indexing
    date.date = days_size + 1;

    return date;
}
