#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#pragma pack(push, 1)
struct BundleFooter {
  uint64_t offset;
  uint64_t size;
  uint32_t magic; // 0x47475546 (GGUF)
};
#pragma pack(pop)

bool set_executable(const char *path) {
#ifdef _WIN32
  return true; // Not strictly needed for .exe on Windows
#else
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  return chmod(path, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) == 0;
#endif
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " <server_exe> <model_gguf> <output_exe>" << std::endl;
    return 1;
  }

  std::string server_path = argv[1];
  std::string model_path = argv[2];
  std::string output_path = argv[3];

  std::ifstream server_file(server_path, std::ios::binary);
  if (!server_file) {
    std::cerr << "Failed to open server executable: " << server_path
              << std::endl;
    return 1;
  }

  std::ifstream model_file(model_path, std::ios::binary);
  if (!model_file) {
    std::cerr << "Failed to open model file: " << model_path << std::endl;
    return 1;
  }

  std::ofstream output_file(output_path, std::ios::binary);
  if (!output_file) {
    std::cerr << "Failed to open output file: " << output_path << std::endl;
    return 1;
  }

  // 1. Copy server
  output_file << server_file.rdbuf();
  uint64_t current_pos = output_file.tellp(); uint64_t model_offset = (current_pos + 4095) & ~4095; if (model_offset > current_pos) { std::vector<char> padding(model_offset - current_pos, 0); output_file.write(padding.data(), padding.size()); }

  // 2. Copy model
  output_file << model_file.rdbuf();
  uint64_t model_size = (uint64_t)output_file.tellp() - model_offset;

  // 3. Append footer
  BundleFooter footer;
  footer.offset = model_offset;
  footer.size = model_size;
  footer.magic = 0x47475546;

  output_file.write(reinterpret_cast<const char *>(&footer), sizeof(footer));

  output_file.close();
  server_file.close();
  model_file.close();

  if (!set_executable(output_path.c_str())) {
    std::cerr << "Warning: Failed to set executable permissions on "
              << output_path << std::endl;
  }

  std::cout << "Successfully bundled " << model_path << " into " << output_path
            << std::endl;
  std::cout << "Model offset: " << model_offset << ", size: " << model_size
            << std::endl;

  return 0;
}
