#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[])
{
  int initial_count, final_count;
  char buf[100];
  int fd;

  initial_count = getreadcount();
  printf("Initial read count: %d\n", initial_count);

  fd = open("testfile", O_CREATE | O_WRONLY);
  if (fd < 0)
  {
    printf("Error creating file\n");
    exit(1);
  }

  write(fd, "This is test data for reading exactly 100 bytes of content to test our getreadcount syscall implementation.", 100);
  close(fd);

  fd = open("testfile", O_RDONLY);
  if (fd < 0)
  {
    printf("Error opening file\n");
    exit(1);
  }

  read(fd, buf, 100);
  close(fd);

  final_count = getreadcount();
  printf("Final read count: %d\n", final_count);
  printf("Difference: %d bytes\n", final_count - initial_count);

  if (final_count - initial_count >= 100)
  {
    printf("SUCCESS: Read count increased correctly\n");
  }
  else
  {
    printf("ERROR: Read count did not increase as expected\n");
  }

  exit(0);
}