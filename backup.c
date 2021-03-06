#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <utime.h>
#include <limits.h>
#include <getopt.h>
#include <sys/stat.h>

#include "main.h"
#include "common.h"
#include "exall.h"
#include "crc32.h"

static void
backup_backupDir(const char* dir);


static char* backup_currentDir = 0;
static char* backup_skipFile = 0;
static char* backup_dirBuffer = 0;
static int backup_prune = 0;
static int backup_crcVerify = 0;

void
backup_cleanup()
{
  if (backup_currentDir) {
    free(backup_currentDir);
    backup_currentDir = 0;
  }

  if (backup_skipFile) {
    free(backup_skipFile);
    backup_skipFile = 0;
  }

  if (backup_dirBuffer) {
    free(backup_dirBuffer);
    backup_dirBuffer = 0;
  }
}


static char*
backup_fullPath(const char* name)
{
  if (!backup_currentDir) {
    return strdup(name);
  }

  char* path = malloc(strlen(backup_currentDir) + strlen(name) + 2);
  if (!path) {
    return NULL;
  }
  if (backup_currentDir[strlen(backup_currentDir)-1] != ':') {
    sprintf(path, "%s/%s", backup_currentDir, name);
  } else {
    sprintf(path, "%s%s", backup_currentDir, name);
  }
  return path;
}


static void
backup_pruneFiles(const char* filename, void* data)
{
  if (strcmp(filename, ".") == 0 ||
      strcmp(filename, "..") == 0 ||
      strcmp(filename, SQUIRT_EXALL_INFO_DIR) == 0) {
    return;
  }
  dir_entry_list_t* list = data;
  dir_entry_t* entry = list->head;
  int found = 0;
  while (entry) {
    if (strcmp(entry->name, filename) == 0) {
     found = 1;
      break;
    }
    entry = entry->next;
  }

  if (!found) {
    char exFilename[PATH_MAX];
    snprintf(exFilename, sizeof(exFilename), "%s%s", SQUIRT_EXALL_INFO_DIR_NAME, filename);
    char* path = backup_fullPath(filename);
    printf("%c[31m%s \xF0\x9F\x92\x80\xF0\x9F\x92\x80\xF0\x9F\x92\x80 REMOVED \xF0\x9F\x92\x80\xF0\x9F\x92\x80\xF0\x9F\x92\x80%c[0m\n", 27, path, 27); // red, utf-8 skulls
    free(path);
    if (unlink(filename) != 0 || unlink(exFilename) != 0) {
      fatalError("failed to remove %s\n", filename);
    }
  }
}

uint32_t
backup_doCrcVerify(const char* path)
{
  uint32_t error = 0;
   uint32_t crc;
   if (crc32_sum(util_amigaBaseName(path), &crc) != 0) {
     fatalError("crc32 failed for %s", util_amigaBaseName(path));
   }
   char buffer[PATH_MAX];
   snprintf(buffer, sizeof(buffer),"ssum \"%s\"", path);
   fflush(stdout);
   char* result = util_execCapture(buffer);
   if (!result) {
     fatalError("remote crc32 failed for %s", util_amigaBaseName(path));
   }
   snprintf(buffer, sizeof(buffer), "%x\n", crc);
   if (strcasecmp(buffer, result) != 0) {
     //     fatalError("crc32 verify failed for %s (%s,%s)", path, buffer, result);
     printf("CRC doesn't match! %s", path);
     error = 1;
   }
   free(result);
   return error;
}

static void
backup_backupList(dir_entry_list_t* list)
{
  dir_entry_t* entry = list->head;

  while (entry) {
    if (entry->type < 0) {
      const char* path = backup_fullPath(entry->name);
      int skipFile = 0;
      if (backup_skipFile) {
	char* found = strstr(backup_skipFile, path);
	if (found) {
	  found += strlen(path);
	  skipFile = *found == 0 || *found == '\n' || *found == '\r';
	}
      }
      int skip = skipFile;

      if (!skipFile) {
	dir_entry_t *temp = dir_newDirEntry();
	struct stat st;
	if (stat(util_amigaBaseName(path), &st) == 0) {
	  if (st.st_size == (off_t)entry->size) {
	    skip = exall_readExAllData(temp, path);
	    if (skip) {
	      skip = exall_identicalExAllData(temp, entry);
	    }
	  }
	}
	dir_freeEntry(temp);

	if (backup_crcVerify) {
	  if (backup_doCrcVerify(path) != 0) {
	    skip = 0;
	  }
	}
      }

      if (skip) {
	if (skipFile) {
	  printf("\xF0\x9F\x9A\xAB %c[1m%s \xE2\x80\x94\xE2\x80\x94\xE2\x80\x94SKIPPED\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94 %c[0m\n", 27, path, 27); // utf-8 no entry bold
	} else {
	  printf("\xE2\x9C\x85 %s\n", path); // utf-8 tick
	}
      } else {
	uint32_t protect;

	char updateMessage[PATH_MAX];
	snprintf(updateMessage, sizeof(updateMessage), "%s saving...", path);

	if (squirt_suckFile(path, updateMessage, restore_printProgress, 0, &protect) < 0) {
	  /*
	    FILE* fp = fopen("skip-entry", "wb+");
	    fprintf(fp, "%s\n", path);
	    fclose(fp);
	  */
	    fatalError("failed to backup %s", path);
	}
	exall_saveExAllData(entry, path);

	if (backup_crcVerify) {
	  if (backup_doCrcVerify(path) != 0) {
	    fatalError("CRC32 verification failed for %s", path);
	  }
	}

#ifndef _WIN32
	printf("\r%c[K", 27);
#else
	printf("\r");
#endif
	printf("\xE2\x9C\x85 %s saving...done  \n", path); // utf-8 tick
	fflush(stdout);
      }
      free((void*)path);
    }
    entry = entry->next;
  }

  entry = list->head;
  while (entry) {
    if (entry->type > 0) {
      const char* path = backup_fullPath(entry->name);
      int skipFile = 0;
      if (backup_skipFile) {
	char* found = strstr(backup_skipFile, path);
	if (found) {
	  found += strlen(path);
	  skipFile = *found == 0 || *found == '\n' || *found == '\r';
	}
      }
      if (!skipFile) {
	backup_backupDir(entry->name);
	exall_saveExAllData(entry, path);
	free((void*)path);
      } else {
	  printf("\xF0\x9F\x9A\xAB %c[1m%s \xE2\x80\x94\xE2\x80\x94\xE2\x80\x94SKIPPED\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94 %c[0m\n", 27, path, 27); // utf-8 no entry bold
	free((void*)path);
      }

    }
    entry = entry->next;
  }

  if (backup_prune) {
    util_dirOperation(".", backup_pruneFiles, list);
  }
}


static char*
backup_pushDir(const char* dir)
{
  if (backup_currentDir) {
    char* newDir = backup_fullPath(dir);
    if (backup_currentDir) {
      free(backup_currentDir);
    }
    backup_currentDir = newDir;
  } else {
    backup_currentDir = malloc(strlen(dir)+1);
    strcpy(backup_currentDir, dir);
  }

  if (util_cd(backup_currentDir) != 0) {
    fatalError("unable to backup %s", backup_currentDir);
  }

  char* safe = util_safeName(dir);
  if (!safe) {
    fatalError("failed to create safe name");
  }

  int mkdirResult = util_mkdir(safe, 0777);

  if (mkdirResult != 0 && errno != EEXIST) {
    fatalError("failed to mkdir %s", safe);
  }

  char* cwd = getcwd(0, 0);

  if (chdir(safe) == -1) {
    fatalError("unable to chdir to %s", safe);
  }

  free(safe);

  return cwd;
}


static void
backup_popDir(char* cwd)
{
  for (int i = strlen(backup_currentDir)-1; i >= 0; --i) {
    if (backup_currentDir[i] == '/' || (i > 0 && backup_currentDir[i-1] == ':')) {
      backup_currentDir[i] = 0;
      break;
    }
  }

  if (chdir(cwd)) {
    fatalError("failed to cd to %s", cwd);
  }
  free((void*)cwd);
}


static void
backup_backupDir(const char* dir)
{
  char* cwd = backup_pushDir(dir);
  printf("\xE2\x9C\x85 %s\n", backup_currentDir); // utf-8 tick
  if (dir_process(backup_currentDir, backup_backupList) != 0) {
    fatalError("unable to read %s", dir);
  }

  backup_popDir(cwd);
}


char*
backup_loadSkipFile(const char* filename, int ignoreErrors)
{
  struct stat st;

  if (stat(filename, &st) == -1) {
    if (!ignoreErrors) {
      fatalError("filed to load skip file: %s", filename);
    }
    return 0;
  }

  int fileLength = st.st_size;
  char* skipFile = malloc(fileLength+1);
  if (skipFile) {
    memset(skipFile, 0, fileLength+1);
    int fd = open(filename,  O_RDONLY|_O_BINARY);
    if (fd) {
      if (read(fd, skipFile, fileLength) != fileLength) {
	close(fd);
	fatalError("failed to read skipfile %s", filename);
      }
    } else {
      fatalError("failed to open skipfile %s", filename);
    }

    close(fd);
  }
  return skipFile;
}


_Noreturn static void
backup_usage(void)
{
  fatalError("invalid arguments\nusage: %s [--crc32] [--prune] [--skipfile=skipfile] hostname dir_name", main_argv0);
}


void
backup_main(int argc, char* argv[])
{
  backup_skipFile = 0;
  backup_currentDir = 0;
  const char* hostname = 0;
  char* path = 0;
  char* skipfile = 0;
  int argvIndex = 1;

  while (argvIndex < argc) {
    static struct option long_options[] =
      {
       {"prune",    no_argument, &backup_prune, 'p'},
       {"crc32",    no_argument, &backup_crcVerify, 'c'},
       {"skipfile", required_argument, 0, 's'},
       {0, 0, 0, 0}
      };
    int option_index = 0;
    int c = getopt_long (argc, argv, "", long_options, &option_index);
    if (c != -1) {
      argvIndex = optind;
      switch (c) {
      case 0:
	break;
      case 's':
	if (optarg == 0 || strlen(optarg) == 0) {
	  backup_usage();
	}
	skipfile = optarg;
	break;
      case '?':
      default:
	backup_usage();
	break;
      }
    } else {
      if (hostname == 0) {
	hostname = argv[argvIndex];
      } else {
	path = argv[argvIndex];
      }
      optind++;
      argvIndex++;
    }
  }

  if (!hostname || !path) {
    backup_usage();
  }

  if (skipfile) {
    backup_skipFile = backup_loadSkipFile(skipfile, 0);
  } else {
    backup_skipFile = backup_loadSkipFile(".skip", 1);
  }

  util_connect(hostname);

  char* token = strtok(path, ":");
  char* dir = 0;
  if (token) {
    dir = token;
    token = strtok(0, "/");
    if (token) {
      backup_dirBuffer = malloc(strlen(dir)+2);
      if (!backup_dirBuffer) {
	fatalError("malloc failed");
      }
      sprintf(backup_dirBuffer, "%s:", dir);
      free(backup_pushDir(backup_dirBuffer));
      do {
	dir = token;
	token = strtok(0, "/");
	if (token) {
	  free(backup_pushDir(dir));
	}
      } while (token);
    } else {
      backup_dirBuffer = malloc(strlen(dir)+2);
      if (!backup_dirBuffer) {
	fatalError("malloc failed");
      }
      sprintf(backup_dirBuffer, "%s:", dir);
      dir = backup_dirBuffer;
    }
  }

  if (dir) {
    backup_backupDir(dir);
  }


  printf("\nbackup complete!\n");
}
