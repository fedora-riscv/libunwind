/*
 * Copyright 2006-2007 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Reap any leftover children possibly holding file descriptors.
 * Children are identified by the stale file descriptor or PGID / SID.
 * Both can be missed but only the stale file descriptors are important for us.
 * PGID / SID may be set by the children on their own.
 * If we fine a candidate we kill it will all its process tree (grandchildren).
 * The child process is run with `2>&1' redirection (due to forkpty(3)).
 * 2007-07-10  Jan Kratochvil  <jan.kratochvil@redhat.com>
 */

/* For getpgid(2).  */
#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <assert.h>
#include <pty.h>
#include <poll.h>

#define LENGTH(x) (sizeof (x) / sizeof (*(x)))

static const char *progname;

static volatile int signal_child_hit = 0;

/* We use it to race-safely emulate ppoll(2) by poll(2).  */
static int pipefd[2];

static void signal_child (int signo)
{
  int i;

  signal_child_hit = 1;

  assert (pipefd[1] != -1);
  i = close (pipefd[1]);
  assert (i == 0);
  pipefd[1] = -1;
}

static volatile int signal_alarm_hit = 0;

static void signal_alarm (int signo)
{
  signal_alarm_hit = 1;
}

static char childptyname[LINE_MAX];
static pid_t child;

static void print_child_error (const char *reason, char **argv)
{
  char **sp;

  fprintf (stderr, "%s: %d %s:", progname, (int) child, reason);
  for (sp = argv; *sp != NULL; sp++)
    {
      fputc (' ', stderr);
      fputs (*sp, stderr);
    }
  fputc ('\n', stderr);
}

static int spawn (char **argv, int timeout)
{
  pid_t child_got;
  int status, amaster, i, rc;
  struct sigaction act;
  struct termios termios;
  unsigned alarm_orig;

  i = pipe (pipefd);
  assert (i == 0);

  /* We do not use signal(2) to be sure we have SA_RESTART set.  */
  memset (&act, 0, sizeof (act));
  act.sa_handler = signal_child;
  i = sigemptyset (&act.sa_mask);
  assert (i == 0);
  act.sa_flags = SA_RESTART;
  i = sigaction (SIGCHLD, &act, NULL);
  assert (i == 0);

  /* With TERMP passed as NULL we get "\n" -> "\r\n".  */
  termios.c_iflag = IGNBRK | IGNPAR;
  termios.c_oflag = 0;
  termios.c_cflag = CS8 | CREAD | CLOCAL | HUPCL | B9600;
  termios.c_lflag = IEXTEN | NOFLSH;
  memset (termios.c_cc, _POSIX_VDISABLE, sizeof (termios.c_cc));
  termios.c_cc[VTIME] = 0;
  termios.c_cc[VMIN ] = 1;
  cfmakeraw (&termios);
#ifdef FLUSHO
  /* Workaround a readline deadlock bug in _get_tty_settings().  */
  termios.c_lflag &= ~FLUSHO;
#endif
  child = forkpty (&amaster, childptyname, &termios, NULL);
  switch (child)
    {
      case -1:
	perror ("forkpty(3)");
	exit (EXIT_FAILURE);
      case 0:
	i = close (pipefd[0]);
	assert (i == 0);
	i = close (pipefd[1]);
	assert (i == 0);

	/* Do not replace STDIN as inferiors query its termios.  */
#if 0
	i = close (STDIN_FILENO);
	assert (i == 0);
	i = open ("/dev/null", O_RDONLY);
	assert (i == STDIN_FILENO);
#endif

	/* Do not setpgrp(2) in the parent process as the process-group
	   is shared for the whole sh(1) pipeline we could be a part
	   of.  The process-group is set according to PID of the first
	   command in the pipeline.
	   We would rip even vi(1) in the case of:
		./orphanripper sh -c 'sleep 1&' | vi -
	   */
	/* Do not setpgrp(2) as our pty would not be ours and we would
	   get `SIGSTOP' later, particularly after spawning gdb(1).
	   setsid(3) was already executed by forkpty(3) and it would fail if
	   executed again.  */
	if (getpid() != getpgrp ())
	  {
	    perror ("getpgrp(2)");
	    exit (EXIT_FAILURE);
	  }
	execvp (argv[0], argv);
	perror ("execvp(2)");
	exit (EXIT_FAILURE);
      default:
	break;
    }
  i = fcntl (amaster, F_SETFL, O_RDWR | O_NONBLOCK);
  if (i != 0)
    {
      perror ("fcntl (amaster, F_SETFL, O_NONBLOCK)");
      exit (EXIT_FAILURE);
    }

  /* We do not use signal(2) to be sure we have SA_RESTART set.  */
  act.sa_handler = signal_alarm;
  act.sa_flags &= ~SA_RESTART;
  i = sigaction (SIGALRM, &act, NULL);
  assert (i == 0);

  alarm_orig = alarm (timeout);
  assert (alarm_orig == 0);

  while (!signal_alarm_hit)
    {
      struct pollfd pollfd[2];
      char buf[LINE_MAX];
      ssize_t buf_got;

      pollfd[0].fd = amaster;
      pollfd[0].events = POLLIN;
      pollfd[1].fd = pipefd[0];
      pollfd[1].events = POLLIN;
      i = poll (pollfd, LENGTH (pollfd), -1);
      if (i == -1 && errno == EINTR)
        {
	  /* Weird but SA_RESTART sometimes does not work.  */
	  continue;
	}
      assert (i >= 1);
      /* Data available?  Process it first.  */
      if (pollfd[0].revents & POLLIN)
	{
	  buf_got = read (amaster, buf, sizeof buf);
	  if (buf_got <= 0)
	    {
	      perror ("read (amaster)");
	      exit (EXIT_FAILURE);
	    }
	  if (write (STDOUT_FILENO, buf, buf_got) != buf_got)
	    {
	      perror ("write(2)");
	      exit (EXIT_FAILURE);
	    }
	}
      if (pollfd[0].revents & POLLHUP)
        break;
      if ((pollfd[0].revents &= ~POLLIN) != 0)
	{
	  fprintf (stderr, "%s: ppoll(2): revents 0x%x\n", progname,
		   (unsigned) pollfd[0].revents);
	  exit (EXIT_FAILURE);
	}
      /* Child exited?  */
      if (pollfd[1].revents & POLLHUP)
	break;
      assert (pollfd[1].revents == 0);
    }

  if (signal_alarm_hit)
    {
      i = kill (child, SIGKILL);
      assert (i == 0);
    }
  else
    alarm (0);

  /* WNOHANG still could fail.  */
  child_got = waitpid (child, &status, 0);
  if (child != child_got)
    {
      fprintf (stderr, "waitpid (%d) = %d: %m\n", (int) child, (int) child_got);
      exit (EXIT_FAILURE);
    }
  if (signal_alarm_hit)
    {
      char *buf;

      if (asprintf (&buf, "Timed out after %d seconds", timeout) != -1)
	{
	  print_child_error (buf, argv);
	  free (buf);
	}
      rc = 128 + SIGALRM;
    }
  else if (WIFEXITED (status))
    rc = WEXITSTATUS (status);
  else if (WIFSIGNALED (status))
    {
      print_child_error (strsignal (WTERMSIG (status)), argv);
      rc = 128 + WTERMSIG (status);
    }
  else if (WIFSTOPPED (status))
    {
      fprintf (stderr, "waitpid (%d): WIFSTOPPED - WSTOPSIG is %d\n",
	       (int) child, WSTOPSIG (status));
      exit (EXIT_FAILURE);
    }
  else
    {
      fprintf (stderr, "waitpid (%d): !WIFEXITED (%d)\n", (int) child, status);
      exit (EXIT_FAILURE);
    }

  assert (signal_child_hit != 0);

  assert (pipefd[1] == -1);
  i = close (pipefd[0]);
  assert (i == 0);

  /* Do not close the master FD as the child would have `/dev/pts/23 (deleted)'
     entries which are not expected (and expecting ` (deleted)' would be
     a race.  */
#if 0
  i = close (amaster);
  if (i != 0)
    {
      perror ("close (forkpty ()'s amaster)");
      exit (EXIT_FAILURE);
    }
#endif

  return rc;
}

/* Detected commandline may look weird due to a race:
   Original command:
	./orphanripper sh -c 'sleep 1&' &
   Correct output:
	[1] 29610
	./orphanripper: Killed -9 orphan PID 29612 (PGID 29611): sleep 1
   Raced output (sh(1) child still did not update its argv[]):
	[1] 29613
	./orphanripper: Killed -9 orphan PID 29615 (PGID 29614): sh -c sleep 1&
   We could delay a bit before ripping the children.  */
static const char *read_cmdline (pid_t pid)
{
  char cmdline_fname[32];
  static char cmdline[LINE_MAX];
  int fd;
  ssize_t got;
  char *s;

  if (snprintf (cmdline_fname, sizeof cmdline_fname, "/proc/%d/cmdline",
      (int) pid) < 0)
    return NULL;
  fd = open (cmdline_fname, O_RDONLY);
  if (fd == -1)
    {
      /* It may have already exited - ENOENT.  */
#if 0
      fprintf (stderr, "%s: open (\"%s\"): %m\n", progname, cmdline_fname);
#endif
      return NULL;
    }
  got = read (fd, cmdline, sizeof (cmdline) - 1);
  if (got == -1)
    fprintf (stderr, "%s: read (\"%s\"): %m\n", progname,
       cmdline_fname);
  if (close (fd) != 0)
    fprintf (stderr, "%s: close (\"%s\"): %m\n", progname,
       cmdline_fname);
  if (got < 0)
    return NULL;
  /* Convert '\0' argument delimiters to spaces.  */
  for (s = cmdline; s < cmdline + got; s++)
    if (!*s)
      *s = ' ';
  /* Trim the trailing spaces (typically single '\0'->' ').  */
  while (s > cmdline && isspace (s[-1]))
    s--;
  *s = 0;
  return cmdline;
}

static int dir_scan (const char *dirname,
		  int (*callback) (struct dirent *dirent, const char *pathname))
{
  DIR *dir;
  struct dirent *dirent;
  int rc = 0;

  dir = opendir (dirname);
  if (dir == NULL)
    {
      if (errno == EACCES || errno == ENOENT)
	return rc;
      fprintf (stderr, "%s: opendir (\"%s\"): %m\n", progname, dirname);
      exit (EXIT_FAILURE);
    }
  while ((errno = 0, dirent = readdir (dir)))
    {
      char pathname[LINE_MAX];
      int pathname_len;

      pathname_len = snprintf (pathname, sizeof pathname, "%s/%s",
				 dirname, dirent->d_name);
      if (pathname_len <= 0 || pathname_len >= (int) sizeof pathname)
	{
	  fprintf (stderr, "entry file name too long: `%s' / `%s'\n",
		   dirname, dirent->d_name);
	  continue;
	}
      /* RHEL-4.5 on s390x never fills in D_TYPE.  */
      if (dirent->d_type == DT_UNKNOWN)
        {
	  struct stat statbuf;
	  int i;

	  /* We are not interested in the /proc/PID/fd/ links targets.  */
	  i = lstat (pathname, &statbuf);
	  if (i == -1)
	    {
	      if (errno == EACCES || errno == ENOENT)
	        continue;
	      fprintf (stderr, "%s: stat (\"%s\"): %m\n", progname, pathname);
	      exit (EXIT_FAILURE);
	    }
	  if (S_ISDIR (statbuf.st_mode))
	    dirent->d_type = DT_DIR;
	  if (S_ISLNK (statbuf.st_mode))
	    dirent->d_type = DT_LNK;
	  /* No other D_TYPE types used in this code.  */
	}
      rc = (*callback) (dirent, pathname);
      if (rc != 0)
	{
	  errno = 0;
	  break;
	}
    }
  if (errno != 0)
    {
      fprintf (stderr, "%s: readdir (\"%s\"): %m\n", progname, dirname);
      exit (EXIT_FAILURE);
    }
  if (closedir (dir) != 0)
    {
      fprintf (stderr, "%s: closedir (\"%s\"): %m\n", progname, dirname);
      exit (EXIT_FAILURE);
    }
  return rc;
}

static int fd_fs_scan (pid_t pid, int (*func) (pid_t pid, const char *link))
{
  char dirname[64];

  if (snprintf (dirname, sizeof dirname, "/proc/%d/fd", (int) pid) < 0)
    {
      perror ("snprintf(3)");
      exit (EXIT_FAILURE);
    }

  int callback (struct dirent *dirent, const char *pathname)
  {
    char buf[LINE_MAX];
    ssize_t buf_len;

    if ((dirent->d_type != DT_DIR && dirent->d_type != DT_LNK)
	|| (dirent->d_type == DT_DIR && strcmp (dirent->d_name, ".") != 0
	    && strcmp (dirent->d_name, "..") != 0)
	|| (dirent->d_type == DT_LNK && strspn (dirent->d_name, "0123456789")
	    != strlen (dirent->d_name)))
      {
	fprintf (stderr, "Unexpected entry \"%s\" (d_type %u)"
			 " on readdir (\"%s\"): %m\n",
		 dirent->d_name, (unsigned) dirent->d_type, dirname);
	return 0;
      }
    if (dirent->d_type == DT_DIR)
      return 0;
    buf_len = readlink (pathname, buf, sizeof buf - 1);
    if (buf_len <= 0 || buf_len >= (ssize_t) sizeof buf - 1)
      {
	if (errno != ENOENT && errno != EACCES)
	  fprintf (stderr, "Error reading link \"%s\": %m\n", pathname);
	return 0;
      }
    buf[buf_len] = 0;
    return (*func) (pid, buf);
  }

  return dir_scan (dirname, callback);
}

static void pid_fs_scan (void (*func) (pid_t pid, void *data), void *data)
{
  int callback (struct dirent *dirent, const char *pathname)
  {
    if (dirent->d_type != DT_DIR
	|| strspn (dirent->d_name, "0123456789") != strlen (dirent->d_name))
      return 0;
    (*func) (atoi (dirent->d_name), data);
    return 0;
  }

  dir_scan ("/proc", callback);
}

static int rip_check_ptyname (pid_t pid, const char *link)
{
  assert (pid != getpid ());

  return strcmp (link, childptyname) == 0;
}

struct pid
  {
    struct pid *next;
    pid_t pid;
  };
static struct pid *pid_list;

static int pid_found (pid_t pid)
{
  struct pid *entry;

  for (entry = pid_list; entry != NULL; entry = entry->next)
    if (entry->pid == pid)
      return 1;
  return 0;
}

/* Single pass is not enough, a (multithreaded) process was seen to survive.
   Repeated killing of the same process is not enough, zombies can be killed.
   */
static int cleanup_acted;

static void pid_record (pid_t pid)
{
  struct pid *entry;

  if (pid_found (pid))
    return;
  cleanup_acted = 1;

  entry = malloc (sizeof (*entry));
  if (entry == NULL)
    {
      fprintf (stderr, "%s: malloc: %m\n", progname);
      exit (EXIT_FAILURE);
    }
  entry->pid = pid;
  entry->next = pid_list;
  pid_list = entry;
}

static void pid_forall (void (*func) (pid_t pid))
{
  struct pid *entry;

  for (entry = pid_list; entry != NULL; entry = entry->next)
    (*func) (entry->pid);
}

/* Returns 0 on failure.  */
static pid_t pid_get_parent (pid_t pid)
{
  char fname[64];
  FILE *f;
  char line[LINE_MAX];
  pid_t retval = 0;

  if (snprintf (fname, sizeof fname, "/proc/%d/status", (int) pid) < 0)
    {
      perror ("snprintf(3)");
      exit (EXIT_FAILURE);
    }
  f = fopen (fname, "r");
  if (f == NULL)
    {
      return 0;
    }
  while (errno = 0, fgets (line, sizeof line, f) == line)
    {
      if (strncmp (line, "PPid:\t", sizeof "PPid:\t" - 1) != 0)
	continue;
      retval = atoi (line + sizeof "PPid:\t" - 1);
      errno = 0;
      break;
    }
  if (errno != 0)
    {
      fprintf (stderr, "%s: fgets (\"%s\"): %m\n", progname, fname);
      exit (EXIT_FAILURE);
    }
  if (fclose (f) != 0)
    {
      fprintf (stderr, "%s: fclose (\"%s\"): %m\n", progname, fname);
      exit (EXIT_FAILURE);
    }
  return retval;
}

static void killtree (pid_t pid);

static void killtree_pid_fs_scan (pid_t pid, void *data)
{
  pid_t parent_pid = *(pid_t *) data;

  /* Do not optimize it as we could miss some newly spawned processes.
     Always traverse all the leaves.  */
#if 0
  /* Optimization.  */
  if (pid_found (pid))
    return;
#endif

  if (pid_get_parent (pid) != parent_pid)
    return;

  killtree (pid);
}

static void killtree (pid_t pid)
{
  pid_record (pid);
  pid_fs_scan (killtree_pid_fs_scan, &pid);
}

static void rip_pid_fs_scan (pid_t pid, void *data)
{
  pid_t pgid;

  /* Shouldn't happen.  */
  if (pid == getpid ())
    return;

  /* Check both PGID and the stale file descriptors.  */
  pgid = getpgid (pid);
  if (pgid == child
      || fd_fs_scan (pid, rip_check_ptyname) != 0)
    killtree (pid);
}

static void killproc (pid_t pid)
{
  const char *cmdline;

  cmdline = read_cmdline (pid);
  /* Avoid printing the message for already gone processes.  */
  if (kill (pid, 0) != 0 && errno == ESRCH)
    return;
  if (cmdline == NULL)
    cmdline = "<error>";
  fprintf (stderr, "%s: Killed -9 orphan PID %d: %s\n", progname, (int) pid, cmdline);
  if (kill (pid, SIGKILL) == 0)
    cleanup_acted = 1;
  else if (errno != ESRCH)
    fprintf (stderr, "%s: kill (%d, SIGKILL): %m\n", progname, (int) pid);
  /* RHEL-3 kernels cannot SIGKILL a `T (stopped)' process.  */
  kill (pid, SIGCONT);
  /* Do not waitpid(2) as it cannot be our direct descendant and it gets
     cleaned up by init(8).  */
#if 0
  pid_t pid_got;
  pid_got = waitpid (pid, NULL, 0);
  if (pid != pid_got)
    {
      fprintf (stderr, "%s: waitpid (%d) != %d: %m\n", progname,
	 (int) pid, (int) pid_got);
      return;
    }
#endif
}

static void rip (void)
{
  cleanup_acted = 0;
  do
    {
      if (cleanup_acted)
        usleep (1000000 / 10);
      cleanup_acted = 0;
      pid_fs_scan (rip_pid_fs_scan, NULL);
      pid_forall (killproc);
    }
  while (cleanup_acted);
}

int main (int argc, char **argv)
{
  int timeout = 0;
  int rc;

  progname = *argv++;
  argc--;

  if (argc < 1 || strcmp (*argv, "-h") == 0
      || strcmp (*argv, "--help") == 0)
    {
      puts ("Syntax: orphanripper [-t <seconds>] <execvp(3) commandline>");
      exit (EXIT_FAILURE);
    }
  if ((*argv)[0] == '-' && (*argv)[1] == 't')
    {
      char *timeout_s = NULL;

      if ((*argv)[2] == 0)
	timeout_s = *++argv;
      else if (isdigit ((*argv)[2]))
	timeout_s = (*argv) + 2;
      if (timeout_s != NULL)
	{
	  long l;
	  char *endptr;

	  argv++;
	  l = strtol (timeout_s, &endptr, 0);
	  timeout = l;
	  if ((endptr != NULL && *endptr != 0) || timeout < 0 || timeout != l)
	    {
	      fprintf (stderr, "%s: Invalid timeout value: %s\n", progname,
		       timeout_s);
	      exit (EXIT_FAILURE);
	    }
	}
    }

  rc = spawn (argv, timeout);
  rip ();
  return rc;
}
