#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

#pragma pack(push, 1)
struct BundleFooter {
  uint64_t model_offset;
  uint64_t model_size;
  uint32_t magic; // 0x47475546 (GGUF)
};
#pragma pack(pop)

// Universal Polyglot Header (Shell + Batch)
const char *UNIVERSAL_HEADER =
    "MZqFpD='''\n"
    "'''\n"
    ":; # Unix Shell Logic\n"
    "case \"$(uname -s)\" in\n"
    "  Linux*)  EXT=\"linux\";;\n"
    "  Darwin*) EXT=\"mac\";;\n"
    "  *)       EXT=\"bin\";;\n"
    "esac\n"
    "TMPBIN=\"/tmp/baremetallama_${EXT}_$(date +%s)\"\n"
    "extract_and_run() {\n"
    "  OFF=$1; SIZ=$2; shift 2;\n"
    "  tail -c +$OFF \"$0\" | head -c $SIZ > \"$TMPBIN\"\n"
    "  chmod +x \"$TMPBIN\"\n"
    "  env BAREMETALLAMA_SOURCE=\"$0\" BAREMETALLAMA_OFFSET=\"%3\" \"$TMPBIN\" "
    "\"$@\"\n"
    "  EXIT_CODE=$?\n"
    "  rm -f \"$TMPBIN\"\n"
    "  exit $EXIT_CODE\n"
    "}\n"
    "# Extraction logic will be patched here by the bundler\n"
    "exit 0\n"
    "/*\r\n"
    "@echo off\r\n"
    "set TMPBIN=%TEMP%\\baremetallama_%RANDOM%.exe\r\n"
    "powershell -Command \"$f=[System.IO.File]::OpenRead('%~f0'); $f.Seek(%1, "
    "[System.IO.SeekOrigin]::Begin); $b=New-Object byte[] %2; $f.Read($b, 0, "
    "%2); [System.IO.File]::WriteAllBytes('%TMPBIN%', $b); $f.Close()\"\r\n"
    "set BAREMETALLAMA_SOURCE=%~f0\r\n"
    "set BAREMETALLAMA_OFFSET=%3\r\n"
    "\"%TMPBIN%\" %*\r\n"
    "del \"%TMPBIN%\"\r\n"
    "goto :EOF\r\n"
    "*/\n";

bool set_executable(const char *path) {
#ifdef _WIN32
  return true;
#else
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  return chmod(path, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) == 0;
#endif
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <server_exe> <model_gguf> [output_file.baremetallama]"
              << std::endl;
    std::cerr << "Note: If output file is not provided, it defaults to "
                 "<model_name>.baremetallama"
              << std::endl;
    return 1;
  }

  std::string server_path = argv[1];
  std::string model_path = argv[2];
  std::string output_path = (argc > 3) ? argv[3] : "";

  if (output_path.empty()) {
    output_path = model_path;
    size_t last_dot = output_path.find_last_of('.');
    if (last_dot != std::string::npos)
      output_path = output_path.substr(0, last_dot);
    output_path += ".baremetallama";
  } else if (output_path.find(".baremetallama") == std::string::npos) {
    output_path += ".baremetallama";
  }

  std::ifstream server_infile(server_path, std::ios::binary);
  std::ifstream model_infile(model_path, std::ios::binary);

  if (!server_infile || !model_infile) {
    std::cerr << "Error opening input files." << std::endl;
    return 1;
  }

  // Get server binary size
  server_infile.seekg(0, std::ios::end);
  uint64_t server_size = server_infile.tellg();
  server_infile.seekg(0, std::ios::beg);

  std::ofstream output_file(output_path, std::ios::binary);
  if (!output_file) {
    std::cerr << "Error opening output file: " << output_path << std::endl;
    return 1;
  }

  // Calculate offsets first
  std::string shebang = "#!/bin/sh\n";
  std::string header_body = UNIVERSAL_HEADER;

  uint64_t multiboot_pos = 64;
  uint32_t multiboot_header[] = {0x1BADB002, 0, (uint32_t)-(0x1BADB002 + 0)};
  uint64_t multiboot_size = sizeof(multiboot_header);

  uint64_t header_total_size = shebang.size() +
                               (multiboot_pos - shebang.size()) +
                               multiboot_size + header_body.size();
  uint64_t binary_offset = (header_total_size + 4095) & ~4095;
  uint64_t model_offset = (binary_offset + server_size + 4095) & ~4095;

  // Patch extraction offsets
  std::string s_off = std::to_string(binary_offset + 1);
  std::string s_size = std::to_string(server_size);
  std::string s_moff = std::to_string(model_offset);

  size_t p = header_body.find("tail -c +$1");
  if (p != std::string::npos)
    header_body.replace(p + 9, 2, s_off);
  p = header_body.find("head -c $2");
  if (p != std::string::npos)
    header_body.replace(p + 8, 2, s_size);
  p = header_body.find("# Extraction logic will be patched here");
  if (p != std::string::npos) {
    size_t end_of_line = header_body.find("\n", p);
    header_body.replace(p, end_of_line - p,
                        "extract_and_run " + s_off + " " + s_size);
  }

  p = header_body.find("Seek(%1");
  if (p != std::string::npos)
    header_body.replace(p + 5, 2, std::to_string(binary_offset));
  p = header_body.find("byte[] %2");
  if (p != std::string::npos)
    header_body.replace(p + 7, 2, s_size);
  p = header_body.find("($b, 0, %2)");
  if (p != std::string::npos)
    header_body.replace(header_body.find("%2", p), 2, s_size);

  while ((p = header_body.find("%3")) != std::string::npos) {
    header_body.replace(p, 2, s_moff);
  }

  // Write all components
  output_file << shebang;
  output_file << header_body;

  // Pad to binary with SPACES (no nulls in the first page!)
  uint64_t cur = output_file.tellp();
  if (binary_offset > cur) {
    std::vector<char> padding(binary_offset - cur, ' ');
    output_file.write(padding.data(), padding.size());
  }

  // Write server binary (payload)
  output_file << server_infile.rdbuf();

  // Pad to model boundary
  cur = output_file.tellp();
  if (model_offset > cur) {
    std::vector<char> padding(model_offset - cur, 0);
    output_file.write(padding.data(), padding.size());
  }

  // Write model data
  output_file << model_infile.rdbuf();
  uint64_t model_size = (uint64_t)output_file.tellp() - model_offset;

  // Append footer
  BundleFooter footer;
  footer.model_offset = model_offset;
  footer.model_size = model_size;
  footer.magic = 0x47475546;

  output_file.write(reinterpret_cast<const char *>(&footer), sizeof(footer));

  output_file.close();
  set_executable(output_path.c_str());

  std::cout << "Successfully created Universal BareMetalLlama: " << output_path
            << std::endl;
  std::cout << "Server payload offset: " << binary_offset
            << ", size: " << server_size << std::endl;
  std::cout << "Model data offset:     " << model_offset
            << ", size: " << model_size << std::endl;

  return 0;
}
