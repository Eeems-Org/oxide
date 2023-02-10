DIST=../../web/dist/liboxide

all: dist

html:
	doxygen Doxyfile

$(DIST): html
	cp -r html $(DIST)

prod: $(DIST)

dev: clean prod
	while inotifywait -e close_write *.h *.cpp examples/*.cpp; do $(MAKE) clean prod; done

clean:
	rm -rf $(DIST)
	rm -rf html

.PHONY: all prod dev clean
