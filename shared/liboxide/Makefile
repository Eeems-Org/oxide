DIST=../../web/dist/liboxide

all: dist

html:
	doxygen Doxyfile

prod: html
	cp -r html $(DIST)

dev:
	while inotifywait -e close_write *.h *.cpp; do doxygen Doxyfile; done

clean:
	rm -r $(DIST)
	rm -r html
