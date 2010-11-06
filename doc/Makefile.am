
EXTRA_DIST = \
	hacking.texi hacking.html hacking.txt \
	xm.txt xi.txt xp.txt

dist-hook:
hacking.html:
	texi2html -monolithic hacking.texi
	makeinfo --no-headers hacking.texi > hacking.txt

clean:
	rm -f hacking.html hacking.txt

