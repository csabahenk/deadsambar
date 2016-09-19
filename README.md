# `dead` does what `dd` eventually aspired to do

These days we rarely want to convert data back
and forth between EBCDIC and ASCII. What `dd` means
for us is not anymore a
[joke on JCL](http://catb.org/jargon/html/D/dd.html),
rather Dump Data (in particular, Dump
Disks) -- and we would like to be enabled to do it
in a robust way.

Unfortunately, as it shockingly dawned on me upon
trying to migrate my data off of an errant disk,
`dd` falls short of this. It either gives up on
facing a read error, or keeps going on nonchalantly,
screwing up offsets (ie. after an unreadable region
of one megabytes, the consequential data will be
shifted one megabyte backwards on the target media,
instead of leaving a one megabyte hole).

So then, in order:
- I cobbled together this program
- I successfully applied it to migrate over my data
  (call unit test)
- I read after [dd on Wikipedia](http://en.wikipedia.org/wiki/Dd_(Unix\))
  upon which I realized that there is a bunch of post-`dd`
  [attempts](http://www.kalysto.org/utilities/dd_rhelp/index.en.html)
  [born](http://garloff.de/kurt/linux/ddrescue/)
  [out](http://gnu.org/software/ddrescue/ddrescue.html)
  [of](http://seed7.sourceforge.net/scrshots/savehd7.htm) the very same
  frustration.

So I could have applied any of these very fine tools... at this
point I would be happy if I could say that this program took less
time to put together than look after prior art on Wikipedia, but
that's not true. Nevertheless, _almost_ true!

- `dead` is dead simple small and stupid, does not try to be
  clever like those tools (I believe you are better at it,
  so I don't try to compete with you)
- `dead` continues in presence of errors, leaving an appropriately
  sized hole in the target media and a log message
- `dead` makes use of the Linux
  [sendfile(2)](http://man7.org/linux/man-pages/man2/sendfile.2.html)
  system call (which has been rewritten to use the
  [splice](http://lwn.net/Articles/178199/) backend
  [as of 2.6.23](http://kernelnewbies.org/Linux_2_6_23#head-c8fd2455c44d9559429c0f72dbc85cd54a62470d)),
  and that makes it _fast_ by direct end-to-end in-kernel data copy.
  ~~I bet aforementioned utils are too old farty to do
  this, although I haven't checked~~. Well actually
  `dd_rescue` supports
  [splice(2)](http://man7.org/linux/man-pages/man2/splice.2.html),
  but that's a rather contrived construct enforcing a sandwich of
  pipe in between two splices, `splice(2)` needing to have a pipe
  as one end.

Note that the the `sendfile(2)` manpage claims that _sendfile()_ also
demands a socket end pre-2.6.33 (most likely in reference to
[v2.6.33-rc1~379^2~71](http://github.com/torvalds/linux/commit/v2.6.33-rc1~379%5E2~71)),
however `dead` works just fine for me on (vanillish) 2.6.32.

## How to dance with `dead`

    $ make dead
    $ ./dead -h

    dead does what dd(1) eventually aspired to do...
    Usage: dead [-opt=val...] [--] [progressbar helper...] < infile > outfile
    Data is transferred from infile to outfile using
    Linux sendfile(2) (yeh, so it's Linux only).

    Options (with values taking case insensitive k,m,g postfixes):
    -size=N		transfer N bytes (default: size of infile)
    -blksize=N	blocksize (default: 4194304)
    -offset=N	start transfer at offset N (default: 0)
    -targetoffset=N	position target file to N (default:
    			same as offset)
    -logfile=F	(default: stderr)

    If progressbar helper utility is given, it will
    be spawned and fed with data describing the
    progress of the operation.

This should be almost clear, with logfile and progressbar worth
for some words.

You are suggested to specify a logfile, that will have a record
of all intervals where error occurred. Now those intervals still
might have some useful data, which does not get transferred due
to the coarse blocksize. So then you should check your disk's
blocksize (

    hdparm -I /dev/<disk> | grep -i 'phys.*sector'

) and re-run `dead` on the reported intervals (making use of
`-offset` and `-size` options) with `-blksize` set to the disk's
blocksize. Then you are covered.

As of progressbar, `dead` does not implement it, rather outsources.
Some progressbar script is provided (`sambar.rb`) that can be specified
as the progressbar helper argument and that will produce you a simple
progress bar; however, you can write a better one. The progress feed
consists of 64 bit signed integers; the very first one specifies the
size of the whole data to be transferred, and the consecutive ones tell
the number of bytes transferred up to that point. In case of error
`dead` sends a negated value so that the progress bar can mark errors.

## ... and the mysterious dead sambar!

[![dead sambar](deadsambar.jpg)](https://web.archive.org/web/20121112192200/http://tribuneindia.com/2012/20120425/himachal.htm#8)
