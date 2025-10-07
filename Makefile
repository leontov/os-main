
CC ?= gcc
CFLAGS ?= -O3 -I backend/include -DNDEBUG
LIB = backend/libkolibri.a
OBJDIR = backend/obj

SRC := $(wildcard backend/src/*.c)
OBJS := $(patsubst backend/src/%.c,$(OBJDIR)/%.o,$(SRC))

APPS = apps/ks_compiler apps/kolibri_node apps/kolibri_infer

.PHONY: all clean
all: $(LIB) $(APPS)

$(OBJDIR)/%.o: backend/src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(LIB): $(OBJS)
	ar rcs $@ $^

apps/ks_compiler: apps/ks_compiler.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ -lm

apps/kolibri_node: apps/kolibri_node.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ -lm -lcrypto

apps/kolibri_infer: apps/kolibri_infer.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ -lm

clean:
	rm -rf $(OBJDIR) $(LIB) $(APPS)
