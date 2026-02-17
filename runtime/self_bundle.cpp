#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

struct BundleFooter {
  uint64_t offset;
  uint64_t size;
  uint32_t magic;
};

#define BUNDLE_MAGIC 0x47475546 // "GGUF"

bool load_embedded(int &fd, BundleFooter &footer) {
  fd = open("/proc/self/exe", O_RDONLY);
  if (fd < 0)
    return false;

  off_t end = lseek(fd, 0, SEEK_END);
  lseek(fd, end - sizeof(BundleFooter), SEEK_SET);

  read(fd, &footer, sizeof(BundleFooter));

  if (footer.magic != BUNDLE_MAGIC)
    return false;
  return true;
}
