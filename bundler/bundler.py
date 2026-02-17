import argparse
import struct
import shutil
import os
import sys

# Magic bytes "GGUF" in hex: 0x47475546
BUNDLE_MAGIC = 0x47475546

def bundle(server_path, model_path, output_path):
    print(f"Bundling {server_path} + {model_path} -> {output_path}")
    
    # Read server
    try:
        with open(server_path, 'rb') as f:
            server_data = f.read()
    except FileNotFoundError:
        print(f"Error: Server file '{server_path}' not found.")
        sys.exit(1)

    # Read model
    try:
        with open(model_path, 'rb') as f:
            model_data = f.read()
    except FileNotFoundError:
        print(f"Error: Model file '{model_path}' not found.")
        sys.exit(1)

    # Calculate offsets
    server_size = len(server_data)
    model_size = len(model_data)
    gguf_offset = server_size
    
    print(f"Server size: {server_size} bytes")
    print(f"Model size: {model_size} bytes")
    print(f"GGUF offset: {gguf_offset}")

    # Write output
    with open(output_path, 'wb') as f:
        f.write(server_data)
        f.write(model_data)
        
        # Write footer
        # struct BundleFooter { uint64_t offset; uint64_t size; uint32_t magic; };
        # Use little-endian standard
        footer = struct.pack('<QQI', gguf_offset, model_size, BUNDLE_MAGIC)
        f.write(footer)

    # Make executable
    st = os.stat(output_path)
    os.chmod(output_path, st.st_mode | 0o111)
    
    print(f"Successfully created bundled executable: {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Bundle a server executable and a GGUF model into a single binary.")
    parser.add_argument("--server", required=True, help="Path to the server executable")
    parser.add_argument("--model", required=True, help="Path to the GGUF model file")
    parser.add_argument("--out", required=True, help="Path for the output bundled executable")
    
    args = parser.parse_args()
    bundle(args.server, args.model, args.out)
