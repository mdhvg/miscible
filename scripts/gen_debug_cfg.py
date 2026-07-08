#!/bin/python3
import pathlib

# Set cwd as project base dirs
import os
__src_path = pathlib.Path(__file__)
os.chdir(str(__src_path.parent.parent.absolute()))

import re
import argparse

def main():
    parser = argparse.ArgumentParser(description="Replace HF/GitHub URLs custom links.")
    parser.add_argument("url", help="The base URL to replace the prefixes with (e.g., localhost:8080)")
    args = parser.parse_args()
    
    # \1 = the repository/project name
    # \2 = the file path/artifact name
    replacements = {
        r'https://huggingface\.co/.+?/(.+?)/resolve/main/(.+)': rf'{args.url}/\1/\2',
        r'https://github\.com/.+?/.+?/releases/download/(.+)': rf'{args.url}/\1'
    }
    
    try:
        # Read the file
        with open("config.release.yaml", 'r', encoding='utf-8') as file:
            content = file.read()
        
        modified_content = content
        for pattern, replacement in replacements.items():
            modified_content = re.sub(pattern, replacement, modified_content)
        
        with open("config.debug.yaml", 'w', encoding='utf-8', newline="\n") as file:
            file.write(modified_content)
            
        print(f"Success: Processed config.debug.yaml using format '{args.url}/[repo]/[file]'")
    except FileNotFoundError:
        print(f"Error: The file config.release.yaml does not exist.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    main()
