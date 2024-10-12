DIST=../../web/dist/libblight

all: prod

html:
	doxygen Doxyfile

$(DIST): html
	cp -r html $(DIST)

prod: $(DIST)

dev: clean prod
	while inotifywait -re close_write,create,delete .; do $(MAKE) clean prod; done

clean:
	rm -rf $(DIST)
	rm -rf html

.PHONY: all prod dev clean
