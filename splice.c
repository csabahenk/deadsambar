#define _GNU_SOURCE
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sendfile.h>


int
parsesize(char *spec, off_t *size, char *errbuf)
{
	char *ep;

	errno = 0;
	*size = strtoll(spec, &ep, 10);
	if (errno || strlen(ep) > 1 || size <= 0)
		goto error;
	switch (*ep) {
	case '\0':
		break;
	case 'k':
	case 'K':
		*size <<= 10;
		break;
	case 'm':
	case 'M':
		*size <<= 20;
		break;
	case 'g':
	case 'G':
		*size <<= 30;
		break;
	default:
		goto error;
	}

	return 0;

 error:
	strcpy(errbuf, "bogus size");
	return -1;
}

char *
strtail(char *str, const char *pattern)
{
	int i = 0;

	for (i = 0; str[i] == pattern[i] && str[i]; i++);

	if (pattern[i] == '\0')
		return str + i;

	return NULL;
}

int
opt_parse(char *opt, FILE **logfile, off_t *size, ssize_t *blksize, char *errbuf)
{
	char *oarg;

	if (strcmp(opt, "-") == 0)
		return 1;

	oarg = strtail(opt, "logfile=");
	if (oarg) {
		*logfile = fopen(oarg, "a");
		if (!*logfile) {
			sprintf(errbuf, "cannot open logfile: %s", strerror(errno));
			return -1;
		}
		return 0;
	}

	oarg = strtail(opt, "size=");
	if (oarg)
		return parsesize(oarg, size, errbuf);

	oarg = strtail(opt, "blksize=");
	if (oarg)
		return parsesize(oarg, blksize, errbuf);

	sprintf(errbuf, "unrecognized option -%s", opt);
	return -1;
}

typedef ssize_t (*writely)(int fd, const void *buf, size_t count);

ssize_t
swallow(int fd, const void *buf, size_t count)
{
	return count;
}

int
main(int argc, char**argv)
{
	int ret, i, j, p[] = {-1, -1};
	FILE *logfile = stderr;
	off_t size = -1;
	ssize_t blksize = 1 << 22;
	int64_t off, xoff, lastoff;
	char errbuf[1024];
	writely w = swallow;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			ret = opt_parse(argv[i] + 1,
				&logfile, &size, &blksize, errbuf);
			if (ret == -1) {
				fprintf(stderr, "%s\n", errbuf);
				return 1;
			}
			for (j = i; j < argc; j++)
				argv[j] = argv[j + 1];
			argc--;
			/* argv shifted, next check should be at i again */
			i--;
			if (ret == 1)
				break;
		}
	}

	if (size == -1) {
		fprintf(stderr, "size needs to be specified\n");
		return 1;
	}

	if (argc > 1) {
		pid_t pid;
		int xp[2];
		char x;

		ret = pipe(p);
		assert(ret == 0);
		ret = pipe(xp);
		assert(ret == 0);
		fcntl(xp[1], F_SETFD, FD_CLOEXEC);

		pid = fork();
		assert(pid != -1);
		if (pid == 0) {
			close(p[1]);
			dup2(p[0], 0);
			close(xp[0]);

			execvp(argv[1], argv + 1);
			write(xp[1], "x", 1);
			return 1;
		}
		close(p[0]);
		close(xp[1]);
		if (read(xp[0], &x, 1) == 1) {
			fprintf(stderr, "can't exec progressbar\n");
			return 1;
		}

		w = write;
		close(xp[0]);
	}		

	w(p[1], &size, sizeof(size));

	lastoff = size % blksize ? (size / blksize) * blksize : size - blksize;
	for (off = 0; off <= lastoff; off += blksize) {
		/* splice is no good as it wants one end to be a pipe
		ret = splice(0, &off, 1, &off, blksize, SPLICE_F_MOVE);
		*/
		xoff = off;
		ret = sendfile(1, 0, &xoff, blksize);
		if (ret < blksize) {
			if (ret == -1) {
				fprintf(logfile, "%ld[:%ld] failed: %s\n",
					off, blksize, strerror(errno));
				xoff *= -1;
			} else if (off + ret < size) { 
				fprintf(logfile, "%ld[:%ld] short read: got %d\n",
					off, blksize, ret);
				xoff *= -1;
			}
			// amend offset for target file
			ret = lseek(1, off + blksize, SEEK_SET);
			assert(ret != -1);
		}
		w(p[1], &xoff, sizeof(xoff));
	}
		
	return 0;
}
