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

typedef struct {
	FILE *logfile;
	off_t size;
	ssize_t blksize;
	off_t off;
	off_t target_off;
} splccnf;

int
opt_parse(char *opt, splccnf *sc, char *errbuf)
{
	char *oarg;

	if (strcmp(opt, "-") == 0)
		return 1;

	oarg = strtail(opt, "logfile=");
	if (oarg) {
		sc->logfile = fopen(oarg, "a");
		if (!sc->logfile) {
			sprintf(errbuf, "cannot open logfile: %s", strerror(errno));
			return -1;
		}
		return 0;
	}

	oarg = strtail(opt, "size=");
	if (oarg)
		return parsesize(oarg, &sc->size, errbuf);

	oarg = strtail(opt, "blksize=");
	if (oarg)
		return parsesize(oarg, &sc->blksize, errbuf);

	oarg = strtail(opt, "offset=");
	if (oarg)
		return parsesize(oarg, &sc->off, errbuf);

	oarg = strtail(opt, "targetoffset=");
	if (oarg)
		return parsesize(oarg, &sc->target_off, errbuf);

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
	splccnf sc = {
		.logfile    = stderr,
		.size       = -1,
		.blksize    = 1 << 22,
		.off        = 0,
		.target_off = -1
	};
	int64_t off, xoff, lastoff;
	char errbuf[1024];
	writely w = swallow;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			ret = opt_parse(argv[i] + 1, &sc, errbuf);
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

	if (sc.size == -1) {
		fprintf(stderr, "size needs to be specified\n");
		return 1;
	}
	if (sc.target_off == -1)
		sc.target_off = sc.off;

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

	w(p[1], &sc.size, sizeof(sc.size));

	lastoff = sc.size % sc.blksize ? (sc.size / sc.blksize) * sc.blksize : sc.size - sc.blksize;
	ret = lseek(1, sc.target_off, SEEK_SET);
	assert(ret != -1);
	for (off = 0; off <= lastoff; off += sc.blksize) {
		/* splice is no good as it wants one end to be a pipe
		ret = splice(0, &off, 1, &off, sc.blksize, SPLICE_F_MOVE);
		*/
		xoff = sc.off + off;
		ret = sendfile(1, 0, &xoff, sc.blksize);
		xoff -= sc.off; // for progress bar
		if (ret < sc.blksize) {
			if (ret == -1) {
				fprintf(sc.logfile, "%ld[:%ld] failed: %s\n",
					off, sc.blksize, strerror(errno));
				xoff *= -1;
			} else if (off + ret < sc.size) {
				fprintf(sc.logfile, "%ld[:%ld] short read: got %d\n",
					off, sc.blksize, ret);
				xoff *= -1;
			}
			// amend offset for target file
			ret = lseek(1, sc.target_off + off + sc.blksize, SEEK_SET);
			assert(ret != -1);
		}
		w(p[1], &xoff, sizeof(xoff));
	}
		
	return 0;
}
