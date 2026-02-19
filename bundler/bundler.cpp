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
#include <sys/stat.h>
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
              << " <server_exe> <model_gguf> [output_file.com]" << std::endl;
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
  } else if (output_path.find(".baremetallama") == std::string::npos &&
             output_path.find(".") == std::string::npos) {
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

  // Write server binary (Payload at START of file for PE compatibility)
  output_file << server_infile.rdbuf();

  // Align model to 64KB boundary for cross-platform mmap compatibility (Windows
  // requirements)
  uint64_t cur = output_file.tellp();
  uint64_t model_offset = (cur + 65535) & ~65535;
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
  footer.magic = 0x47475546; // GGUF magic in footer too

  output_file.write(reinterpret_cast<const char *>(&footer), sizeof(footer));

  output_file.close();
  set_executable(output_path.c_str());

  std::cout << "Successfully created Universal BareMetalLlama: " << output_path
            << std::endl;
  std::cout << "Model data offset: " << model_offset << ", size: " << model_size
            << std::endl;
  std::cout << "This file is now a valid APE binary and should run on Linux, "
               "Windows, Mac."
            << std::endl;

  return 0;
}
