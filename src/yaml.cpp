#include "yaml.h"

#include "base/log.h"

String _yaml_scan_string(Arena *arena, fy_node *root, U8 *str_buf, const char *node_selector, const char *text_selector)
{
    U64 str_len = 0;

    Assert(fy_node_scanf(root, text_selector, str_buf) > 0, "scanf failed");

    fy_node *node = fy_node_by_path(root, node_selector, FY_NT, FYNWF_FOLLOW);
    Assert(node, "node is NULL");
    Assert(fy_node_get_scalar(node, &str_len), "fy_node_get_scalar(node, &str_len) is NULL");
    return string_cpy(arena, {.v = str_buf, .size = str_len});
}

U64 _yaml_scan_int(fy_node *root, const char *text_selector)
{
    U64 val = 0;
    Assert(fy_node_scanf(root, text_selector, &val), "scanf failed");
    return val;
}

F32 _yaml_scan_float(fy_node *root, const char *text_selector)
{
    F32 val = 0;
    Assert(fy_node_scanf(root, text_selector, &val), "scanf failed");
    return val;
}

void yaml_scan_hash(fy_node *root, U8 *str_buf, U8 *hash_buf, U64 digest_size, const char *text_selector)
{
    fy_node *node = fy_node_by_path(root, text_selector, FY_NT, FYNWF_FOLLOW);
    Assert(node, "invalid node %s", text_selector);
    B32 res = fy_node_scanf(node, "%s", str_buf) > 0;
    Assert(res, "scanf failed");

    for (U32 i = 0; i < digest_size * 2; i += 2)
    {
        U8 v0 = str_buf[i];
        U8 v1 = str_buf[i + 1];

        U8 res = 0;
        if (v0 >= '0' && v0 <= '9')
        {
            res |= v0 - '0';
        }
        else if (v0 >= 'a' && v0 <= 'f')
        {
            res |= v0 - 'a' + 10;
        }
        else
        {
            Assert(0, "v0 is corrupt %d", v0);
        }

        res <<= 4;

        if (v1 >= '0' && v1 <= '9')
        {
            res |= v1 - '0';
        }
        else if (v1 >= 'a' && v1 <= 'f')
        {
            res |= v1 - 'a' + 10;
        }
        else
        {
            Assert(0, "v1 is corrupt %d", v1);
        }

        hash_buf[i / 2] = res;
    }
}
