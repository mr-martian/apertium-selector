AM_LDFLAGS=$(LIBS)

bin_PROGRAMS = apertium-selector apertium-compile-selector

SHARED_CC = lu.cc feature_set.cc pattern_matcher.cc

apertium_selector_SOURCES = apertium_selector.cc selector.cc $(SHARED_CC)

apertium_compile_selector_SOURCES = apertium_compile_selector.cc $(SHARED_CC)
