import json
import argparse

def hex_array_str(hex_str):
    if not hex_str:
        return "{ 0 }"
    bytes_list = [f"0x{hex_str[i:i+2]}" for i in range(0, len(hex_str), 2)]
    return "{ " + ", ".join(bytes_list) + " }"

def generate_c_code(json_path, output_path):
    with open(json_path, 'r') as f:
        data = json.load(f)

    ggml_groups = data.get('GGML', [])
    onnx_groups = data.get('ONNX', [])

    master_files = []

    def add_files_to_master(files_list):
        if not files_list:
            return 0, 0
        start_idx = len(master_files)
        for file in files_list:
            master_files.append({
                'size': file.get('Size', 0),
                'url': file.get('URL', ''),
                'name': file.get('Name', ''),
                'hash': file.get('Hash', '')
            })
        return start_idx, len(files_list)

    # Process GGML groups
    processed_ggml = []
    for group in ggml_groups:
        common_start, common_count = add_files_to_master(group.get('Files', []))
        processed_ggml.append({
            'name': group.get('Name'),
            'common_start': common_start,
            'common_count': common_count,
        })

    # Process ONNX groups & variants
    processed_onnx = []
    for group in onnx_groups:
        common_start, common_count = add_files_to_master(group.get('Files', []))

        variants_data = []
        for var in group.get('Variants', []):
            text_start, text_count = add_files_to_master(var.get('Text', []))
            vision_start, vision_count = add_files_to_master(var.get('Vision', []))
            variants_data.append({
                'precision': var.get('Precision'),
                'text_start': text_start,
                'text_count': text_count,
                'vision_start': vision_start,
                'vision_count': vision_count
            })

        processed_onnx.append({
            'name': group.get('Name'),
            'common_start': common_start,
            'common_count': common_count,
            'variants': variants_data
        })

    # --- Generate C++ Code ---
    out = []
    out.append("// ============================================================================")
    out.append("// AUTO-GENERATED FILE FROM JSON - DO NOT EDIT MANUALLY")
    out.append("// ============================================================================")
    out.append('#include "inference/manifest.h"')
    out.append("")

    # 1. Master Flat Array
    out.append(f"RemoteFile master_files[{len(master_files)}] = {{")
    for i, file in enumerate(master_files):
        hash_c = hex_array_str(file['hash'])
        out.append(f"    // [{i}] {file['name']}")
        out.append("    {")
        out.append(f"        .size = {file['size']},")
        out.append(f'        .url  = sv("{file["url"]}"),')
        out.append(f'        .name = sv("{file["name"]}"),')
        out.append(f"        .hash = {hash_c}")
        out.append("    },")
    out.append("};")
    out.append("")

    # 2. GGML Groups
    out.append(f"U64 ggml_groups_size = {len(processed_ggml)};")
    out.append(f"ModelGroup ggml_groups[{len(processed_ggml)}] = {{")
    for group in processed_ggml:
        common_ptr = f"&master_files[{group['common_start']}]" if group['common_count'] > 0 else "NULL"
        out.append("    {")
        out.append(f'        .name              = sv("{group["name"]}"),')
        out.append(f"        .common_files      = {common_ptr},")
        out.append(f"        .common_files_size = {group['common_count']},")
        out.append("        .variants          = NULL,")
        out.append("        .variants_size     = 0,")
        out.append("    },")
    out.append("};")
    out.append("")

    # Generate static variant arrays beforehand to satisfy C++ rules safely
    for i, group in enumerate(processed_onnx):
        if group['variants']:
            var_array_name = f"onnx_group_{i}_variants"
            out.append(f"static ModelVariant {var_array_name}[{len(group['variants'])}] = {{")
            for v in group['variants']:
                text_ptr = f"&master_files[{v['text_start']}]" if v['text_count'] > 0 else "NULL"
                vision_ptr = f"&master_files[{v['vision_start']}]" if v['vision_count'] > 0 else "NULL"
                out.append("    {")
                out.append(f"        .precision   = {v['precision']},")
                out.append(f"        .text        = {text_ptr},")
                out.append(f"        .text_size   = {v['text_count']},")
                out.append(f"        .vision      = {vision_ptr},")
                out.append(f"        .vision_size = {v['vision_count']},")
                out.append("    },")
            out.append("};")
            out.append("")

    # 3. ONNX Groups & Static Variants Arrays
    out.append(f"U64 onnx_groups_size = {len(processed_onnx)};")

    out.append(f"ModelGroup onnx_groups[{len(processed_onnx)}] = {{")
    for i, group in enumerate(processed_onnx):
        common_ptr = f"&master_files[{group['common_start']}]" if group['common_count'] > 0 else "NULL"
        var_array_name = f"onnx_group_{i}_variants" if group['variants'] else "NULL"
        var_size = len(group['variants']) if group['variants'] else 0

        out.append("    {")
        out.append(f'        .name              = sv("{group["name"]}"),')
        out.append(f"        .common_files      = {common_ptr},")
        out.append(f"        .common_files_size = {group['common_count']},")
        out.append(f"        .variants          = {var_array_name},")
        out.append(f"        .variants_size     = {var_size},")
        out.append("    },")
    out.append("};")

    with open(output_path, 'w', newline='\n') as f:
        f.write("\n".join(out))
    print(f"Generated: {json_path} -> {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert models JSON to a flattened C source file.")
    parser.add_argument("json_input", help="Path to the input models JSON file")
    parser.add_argument("c_output", help="Path to the output C/C++ source file")

    args = parser.parse_args()
    generate_c_code(args.json_input, args.c_output)
