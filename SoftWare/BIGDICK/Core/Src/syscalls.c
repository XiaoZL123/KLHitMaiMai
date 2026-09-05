

#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>

extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

char *__env[1] = { 0 };
char **environ = __env;

void initialise_monitor_handles()
{
}

int _getpid(void)
{
  return 1;
}

int _kill(int pid, int sig)
{
  (void)pid;
  (void)sig;
  errno = EINVAL;
  return -1;
}

void _exit (int status)
{
  _kill(status, -1);
  while (1) {}    
}

__attribute__((weak)) int _read(int file, char *ptr, int len)
{
  (void)file;
  int DataIdx;

  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    *ptr++ = __io_getchar();
  }

  return len;
}

__attribute__((weak)) int _write(int file, char *ptr, int len)
{
  (void)file;
  int DataIdx;

  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    __io_putchar(*ptr++);
  }
  return len;
}

int _close(int file)
{
  (void)file;
  return -1;
}

int _fstat(int file, struct stat *st)
{
  (void)file;
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int file)
{
  (void)file;
  return 1;
}

int _lseek(int file, int ptr, int dir)
{
  (void)file;
  (void)ptr;
  (void)dir;
  return 0;
}

int _open(char *path, int flags, ...)
{
  (void)path;
  (void)flags;

  return -1;
}

int _wait(int *status)
{
  (void)status;
  errno = ECHILD;
  return -1;
}

int _unlink(char *name)
{
  (void)name;
  errno = ENOENT;
  return -1;
}

clock_t _times(struct tms *buf)
{
  (void)buf;
  return -1;
}

int _stat(const char *file, struct stat *st)
{
  (void)file;
  st->st_mode = S_IFCHR;
  return 0;
}

int _link(char *old, char *new)
{
  (void)old;
  (void)new;
  errno = EMLINK;
  return -1;
}

int _fork(void)
{
  errno = EAGAIN;
  return -1;
}

int _execve(char *name, char **argv, char **env)
{
  (void)name;
  (void)argv;
  (void)env;
  errno = ENOMEM;
  return -1;
}

#if defined(__PICOLIBC__)

static int starm_putc(char c, FILE *file)
{
	(void) file;
  __io_putchar(c);
	return c;
}

static int starm_getc(FILE *file)
{
	unsigned char c;
	(void) file;
  c = __io_getchar();
	return c;
}

static FILE __stdio = FDEV_SETUP_STREAM(starm_putc,
					starm_getc,
					NULL,
					_FDEV_SETUP_RW);

FILE *const stdin = &__stdio;
__strong_reference(stdin, stdout);
__strong_reference(stdin, stderr);

__strong_reference(_read, read);
__strong_reference(_write, write);
__strong_reference(_times, times);
__strong_reference(_execve, execve);
__strong_reference(_fork, fork);
__strong_reference(_link, link);
__strong_reference(_unlink, unlink);
__strong_reference(_stat, stat);
__strong_reference(_wait, wait);
__strong_reference(_open, open);
__strong_reference(_close, close);
__strong_reference(_lseek, lseek);
__strong_reference(_isatty, isatty);
__strong_reference(_fstat, fstat);
__strong_reference(_exit, exit);
__strong_reference(_kill, kill);
__strong_reference(_getpid, getpid);

#endif
