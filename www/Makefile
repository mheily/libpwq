# For information about using M4 to generate HTML, see:
#
#     http://tldp.org/LDP/LG/issue22/using_m4.html
#

OBJS=index.html
DESTDIR=~/public_html/proj/libpthread_workqueue

all: clean $(OBJS)

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
	scp *.html *.css heily.com:$(DESTDIR)
