# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

#TODO: revive test-btree

default: runtests

TOP ?= $(abspath ../..)
MODULE = test/kdb

TEST_TOOLS = \
    test-kdb \
    test-wkdb

include $(TOP)/build/Makefile.env

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(ALL_LIBS) $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# test btree
#
TEST_BTREE_SRC = \
	test-btree

TEST_BTREE_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_BTREE_SRC))

TEST_BTREE_LIB = \
	-skapp \
	-sncbi-vdb

$(BINDIR)/test-btree: $(TEST_BTREE_OBJ)
	$(LD) --exe -o $@ $^ $(TEST_BTREE_LIB)

#-------------------------------------------------------------------------------
# test-kdb
#
TEST_KDB_SRC = \
	kdbtest 

TEST_KDB_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_KDB_SRC))

TEST_KDB_LIB = \
    -skapp \
    -sktst \
	-sncbi-vdb

$(TEST_BINDIR)/test-kdb: $(TEST_KDB_OBJ)
	$(LP) --exe -o $@ $^ $(TEST_KDB_LIB)

valgrind_kdb: test-kdb
	valgrind --ncbi $(TEST_BINDIR)/test-kdb

#-------------------------------------------------------------------------------
# test-wkdb
#
TEST_WKDB_SRC = \
	wkdbtest 

TEST_WKDB_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_WKDB_SRC))

TEST_WKDB_LIB = \
    -skapp \
    -sktst \
	-sncbi-wvdb

$(TEST_BINDIR)/test-wkdb: $(TEST_WKDB_OBJ)
	$(LP) --exe -o $@ $^ $(TEST_WKDB_LIB)

valgrind_wkdb: test-wkdb
	valgrind --ncbi $(TEST_BINDIR)/test-wkdb
