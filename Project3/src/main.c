#include <stdio.h>
#include <stdlib.h>

static void repl();
static int runfile(const char* path);

int main(int argc, char** argv)
{
  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    return runfile(argv[1]);
  } else {
    fprintf(stderr, "expected zero or one arguments.\n");
    exit(64);
  }
}

void repl()
{
  char* line = NULL;
  size_t sz = 0;
  for(;;) {
    printf("$ ");
    long read = getline(&line, &sz, stdin);
    if (read == -1) break;

    interpret(line);
  }
}

char* read_file(const char* path)
{
  FILE* file = fopen(path, "rb");
  if (file == NULL) return NULL;
  fseek(file, 0l, SEEK_END);
  size_t sz = ftell(file);
  rewind(file);
  char* buff = (char*)malloc(sz+1);
  size_t bytes_read = fread(buff, sizeof(char), sz, file);
  if (bytes_read < sz) {
    free(buff);
    return NULL;
  }
  buff[bytes_read] = '\0';
  fclose(file);
  return buff;
}

int runfile(const char* path)
{
  char* src  = read_file(path);
  if (src == NULL) {
    fprintf(stderr, "couldn't open file: %s", path);
    return 74;
  } 
  interpret(src);
  free(src);
}
