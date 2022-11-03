AM_LDFLAGS=$(LIBS)

bin_PROGRAMS = apertium-selector apertium-compile-selector apertium-train-selector apertium-train-embeddings

SHARED_CC = lu.cc feature_set.cc pattern_matcher.cc

apertium_selector_SOURCES = apertium_selector.cc selector.cc $(SHARED_CC)

apertium_compile_selector_SOURCES = apertium_compile_selector.cc $(SHARED_CC)

apertium_train_selector_SOURCES = apertium_train_selector.cc train.cc $(SHARED_CC)

apertium_train_embeddings_SOURCES = apertium_train_embeddings.cc embedding_trainer.cc $(SHARED_CC)
