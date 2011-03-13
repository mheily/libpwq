# For information about using M4 to generate HTML, see:
#
#     http://tldp.org/LDP/LG/issue22/using_m4.html
#

OBJS=index.html download.html manpage.html manpage.ps
DESTDIR=~/public_html/proj/libpthread_workqueue

all: clean $(OBJS)

manpage.html:
	groff -mdoc -T html ../pthread_workqueue.3 > manpage.html

manpage.ps:
	groff -mdoc ../pthread_workqueue.3 > manpage.ps

%.html:
	m4 -P < $@.in > $@
	tidy -q -m $@
	chmod 444 $@

clean:
	rm -f $(OBJS)

edit:
	$(EDITOR) *.in

install:
	cd $(DESTDIR) && rm -f $(OBJS)
	cp *.html *.css *.png ~/public_html/libkqueue/

upload: $(OBJS)
	ssh heily.com rm -f $(DESTDIR)/*.html
	scp *.html *.css *.ps heily.com:$(DESTDIR)
