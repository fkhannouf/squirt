#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/socket.h>

static const char*
commandName(struct Process *proc)
{
  struct CommandLineInterface* cli = BADDR(proc->pr_CLI);
  static char buffer[256];
  char* src = BADDR(cli->cli_CommandName);
  char* dest = buffer;
  for (; *src != 0 && dest < &buffer[sizeof(buffer)-2]; src++) {
    if (*src >=  '!') {
      *dest++ = *src;
    }
  }
  *dest = 0;
  return buffer;
}


static void
listCli(void)
{
#if defined(AMIGAOS4)
  for (int i = 0; i < (int)IDOS->MaxCli(); i++) {
    struct Process * proc =  IDOS->FindCliProc(i);
#else
  for (int i = 0; i < (int)MaxCli(); i++) {
    struct Process * proc =  FindCliProc(i);
#endif
    if (proc) {
#if defined(AMIGAOS4)
      IDOS->Printf("[%ld] %s (%s)\n", i, (int)proc->pr_Task.tc_Node.ln_Name, (int)(char*)commandName(proc));
#else
      Printf((APTR)"[%ld] %s (%s)\n", i, (int)proc->pr_Task.tc_Node.ln_Name, (int)(char*)commandName(proc));
#endif
    }
  }
}


int
main(int argc, char **argv)
{
  (void)argc,(void)argv;
#if defined(AMIGAOS4)
  IExec->Forbid();
#else
  Forbid();
#endif
  listCli();
#if defined(AMIGAOS4)
  IExec->Permit();
#else
  Permit();
#endif
  return 0;
}
