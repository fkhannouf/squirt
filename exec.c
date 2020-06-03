#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "argv.h"
#include "main.h"
#include "common.h"

static int exec_socketFd = 0;
static char* exec_command = 0;


void
exec_cleanup(void)
{
  if (exec_command) {
    free(exec_command);
    exec_command = 0;
  }

  if (exec_socketFd > 0) {
    close(exec_socketFd);
    exec_socketFd = 0;
  }
}


int
exec_cmd(const char* hostname, int argc, char** argv)
{
  uint8_t commandCode;
  int commandLength = 0;
  exec_socketFd = 0;
  exec_command = 0;

  if (argc == 2 && strcmp("cd", argv[0]) == 0) {
    commandLength = strlen(argv[1]);
    exec_command = malloc(commandLength+1);
    strcpy(exec_command, argv[1]);
    commandCode = SQUIRT_COMMAND_CD;
  } else {
    for (int i = 0; i < argc; i++) {
      commandLength += strlen(argv[i]);
      commandLength++;
    }

    exec_command = malloc(commandLength+1);
    strcpy(exec_command, argv[0]);
    for (int i = 1; i < argc; i++) {
      strcat(exec_command, " ");
      strcat(exec_command, argv[i]);
    }
    commandCode = SQUIRT_COMMAND_CLI;
  }

  if ((exec_socketFd = util_connect(hostname, commandCode)) < 0) {
    fatalError("failed to connect to squirtd server");
  }

  if (util_sendLengthAndUtf8StringAsLatin1(exec_socketFd, exec_command) != 0) {
    fatalError("send() command failed");
  }

  if (commandCode != SQUIRT_COMMAND_CD) {
    uint8_t c;
#ifdef _WIN32
    char buffer[20];
    int bindex = 0;
#endif
    int exitState = 0;
    while (util_recv(exec_socketFd, &c, 1, 0)) {
      if (c == 0) {
	exitState++;
	if (exitState == 4) {
	  break;
	}
      } else if (c == 0x9B) {
	fprintf(stdout, "%c[", 27);
	fflush(stdout);
      } else {
#ifdef _WIN32
	buffer[bindex++] = c;
	if (bindex == sizeof(buffer)) {
	  write(1, buffer, bindex);
	  bindex = 0;
	}
#else
	int ignore = write(1, &c, 1);
	(void)ignore;
#endif
      }
    }

#ifdef _WIN32
    if (bindex) {
      write(1, buffer, bindex);
    }
#endif
  }

  uint32_t error;

  if (util_recvU32(exec_socketFd, &error) != 0) {
    fatalError("exec: failed to read remote status");
  }

  exec_cleanup();

  return error;
}


void
exec_main(int argc, char* argv[])
{
  if (argc < 3) {
    fatalError("incorrect number of arguments\nusage: %s hostname command to be executed", argv[0]);
  }

  const char* hostname = argv[1];

  for (int i = 0; i < argc-2; i++) {
    argv[i] = argv[i+2];
  }

  argc-=2;

  uint32_t error = exec_cmd(hostname, argc, argv);

  if (error != 0) {
    fatalError("%s", util_getErrorString(error));
  }
}