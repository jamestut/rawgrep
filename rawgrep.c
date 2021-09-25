#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE (64*1024*1024)

const uint8_t hexmap[256] = {
  [0 ... 255]=0xff,
  ['0']=0x0,
  ['1']=0x1,
  ['2']=0x2,
  ['3']=0x3,
  ['4']=0x4,
  ['5']=0x5,
  ['6']=0x6,
  ['7']=0x7,
  ['8']=0x8,
  ['9']=0x9,
  ['a']=0xa,
  ['b']=0xb,
  ['c']=0xc,
  ['d']=0xd,
  ['e']=0xe,
  ['f']=0xf,
  ['A']=0xa,
  ['B']=0xb,
  ['C']=0xc,
  ['D']=0xd,
  ['E']=0xe,
  ['F']=0xf,
};

void print_usage() {
  puts("Usage:");
  puts("  rawgrep -pf (pattern_file) (files...)");
  puts("  rawgrep (pattern_hex) (files...)");
  puts("    (pattern_hex) is a hex string such as '454c46'.");
}

void puts_err(const char* msg) {
  fputs(msg, stderr);
  fputc('\n', stderr);
}

bool scan_hex_input(const char* ipt, size_t* pattern_len, void** pattern_data) {
  uint8_t* ptbptr;
  *pattern_len = strlen(ipt);
  if (!*pattern_len || (*pattern_len % 2)) {
    puts_err("Invalid pattern length.");
    return false;
  }
  *pattern_len /= 2;
  ptbptr = *pattern_data = malloc(*pattern_len);
  if (!ptbptr) {
    puts_err("Error allocating memory for pattern.");
    return false;
  }
  for (size_t i = 0; i < *pattern_len; ++i) {
    uint8_t a = hexmap[(off_t)ipt[i * 2]];
    uint8_t b = hexmap[(off_t)ipt[i * 2 + 1]];
    if ((a | b) == 0xff) {
      puts_err("Invalid hex character.");
      return false;
    }
    ptbptr[i] = (a << 4) | b;
  }
  return true;
}

bool read_pattern_file(const char* fn, size_t* pattern_len, void** pattern_data) {
  struct stat pattern_file;
  int fd;
  if ((fd = open(fn, O_RDONLY)) < 0) {
    perror("Error opening pattern file");
    return false;
  }
  if (fstat(fd, &pattern_file) < 0) {
    perror("Error stat pattern file");
    return false;
  }
  *pattern_len = pattern_file.st_size;
  if (!pattern_file.st_size) {
    puts_err("Pattern file is empty or cannot determine the size.");
    return false;
  }
  *pattern_data = malloc(*pattern_len);
  if (!*pattern_data) {
    puts_err("Error allocating memory for pattern.");
    return false;
  }
  if (read(fd, *pattern_data, *pattern_len) != *pattern_len) {
    puts_err("Error reading pattern file.");
    return false;
  }
  close(fd);
  return true;
}

bool pattern_exists(int fd, const size_t pattern_len, const void* pattern_data) {
  static uint8_t buff[BUFFER_SIZE];
  off_t buff_offset = 0;
  ssize_t readsz;
  size_t buffsz;

  while(1) {
    readsz = read(fd, buff + buff_offset, BUFFER_SIZE - buff_offset);
    if (readsz < 0) {
      readsz = 0;
    }
    buffsz = (size_t)readsz + buff_offset;
    if ((buffsz) < pattern_len) {
      return false;
    }
    if (memmem(buff, buffsz, pattern_data, pattern_len)) {
      return true;
    }
    buff_offset = pattern_len - 1;
    memcpy(buff, buff + buffsz - buff_offset, buff_offset);
  }
}

int main(int argc, char ** argv) {
  size_t pattern_len;
  void* pattern_data;
  // the index where the file list begin
  int argc_files = 3;

  if (argc < argc_files) {
    print_usage();
    return 0;
  }

  if (!strcmp(argv[1], "-pf")) {
    ++argc_files;
    if (argc < argc_files) {
      print_usage();
      return 0;
    }
    if (!read_pattern_file(argv[2], &pattern_len, &pattern_data)) {
      return 1;
    }
  } else {
    if (!scan_hex_input(argv[1], &pattern_len, &pattern_data)) {
      return 1;
    }
  }

  if (pattern_len > (BUFFER_SIZE / 2)) {
    puts_err("Pattern too long.");
    return 1;
  }

  --argc_files;
  for(int i = argc_files; i < argc; ++i) {
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      const char* strerr = strerror(errno);
      fprintf(stderr, "Error opening %s: %s\n", argv[i],
        strerr ? strerr : "(unknown error)");
      continue;
    }
    if (pattern_exists(fd, pattern_len, pattern_data)) {
      printf("match: %s\n", argv[i]);
    }
    close(fd);
  }
  return 0;
}