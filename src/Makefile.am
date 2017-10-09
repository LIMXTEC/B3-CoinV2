AM_CPPFLAGS =  $(INCLUDES) -I$(top_builddir)/src/obj \
  -I$(top_srcdir)/src/leveldb/include -I$(top_srcdir)/src/leveldb/helpers \
  $(BOOST_INCLUDES)
AM_LDFLAGS = $(PTHREAD_CFLAGS)

noinst_LIBRARIES = libbitcoin.a

bin_PROGRAMS = b3coind

SUBDIRS = . $(BUILD_QT) $(BUILD_TEST)
DIST_SUBDIRS = . qt test
.PHONY: FORCE
# bitcoin core #
BITCOIN_CORE_H = alert.h \
    addrman.h \
    base58.h \
    bignum.h \
    chainparams.h \
    chainparamsseeds.h \
    checkpoints.h \
    compat.h \
    coincontrol.h \
    sync.h \
    util.h \
    hash.h \
    uint256.h \
    kernel.h \
    scrypt.h \
    pbkdf2.h \
    serialize.h \
    core.h \
    main.h \
    miner.h \
    net.h \
    key.h \
    db.h \
    txdb.h \
    txmempool.h \
    walletdb.h \
    script.h \
    init.h \
    mruset.h \
	wallet.h \
    keystore.h \
	rpcclient.h \
    rpcprotocol.h \
    rpcserver.h \
    timedata.h \
	crypter.h \
	protocol.h \
	allocators.h \
	ui_interface.h \
	version.h \
    netbase.h \
    clientversion.h \
    threadsafety.h \
    tinyformat.h

JSON_H = json/json_spirit.h json/json_spirit_error_position.h \
  json/json_spirit_reader.h json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h json/json_spirit_utils.h \
  json/json_spirit_value.h json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(abs_top_builddir)/src/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
version.o: obj/build.h

libbitcoin_a_SOURCES = alert.cpp \
    chainparams.cpp \
    version.cpp \
    sync.cpp \
    txmempool.cpp \
    util.cpp \
    hash.cpp \
    netbase.cpp \
    key.cpp \
    script.cpp \
    core.cpp \
    main.cpp \
    miner.cpp \
    init.cpp \
    net.cpp \
    checkpoints.cpp \
    addrman.cpp \
    db.cpp \
    walletdb.cpp \
	wallet.cpp \
    keystore.cpp \
	rpcclient.cpp \
    rpcprotocol.cpp \
    rpcserver.cpp \
    rpcdump.cpp \
    rpcmisc.cpp \
    rpcnet.cpp \
    rpcmining.cpp \
    rpcwallet.cpp \
    rpcblockchain.cpp \
    rpcrawtransaction.cpp \
    timedata.cpp \
	crypter.cpp \
	protocol.cpp \
	noui.cpp \
    kernel.cpp \
    scrypt-arm.S \
    scrypt-x86.S \
    scrypt-x86_64.S \
    scrypt.cpp \
    pbkdf2.cpp
  $(JSON_H) \
  $(BITCOIN_CORE_H)

nodist_libbitcoin_a_SOURCES = $(top_srcdir)/src/obj/build.h
#

# b3coind binary #
b3coind_LDADD = libbitcoin.a leveldb/libleveldb.a leveldb/libmemenv.a \
  $(BOOST_LIBS)
b3coind_SOURCES = bitcoind.cpp
#

leveldb/libleveldb.a:
	@echo "Building LevelDB ..." && cd leveldb && CXX="$(CXX)" CC="$(CC)" \
	  PLATFORM=$(TARGET_OS) AR="$(AR)" $(MAKE) $(LEVELDB_TARGET_FLAGS) OPT="$(CXXFLAGS) $(CPPFLAGS)" libleveldb.a

leveldb/libmemenv.a:
	@echo "Building LevelDB ..." && cd leveldb && CXX="$(CXX)" CC="$(CC)" \
	  PLATFORM=$(TARGET_OS) AR="$(AR)" $(MAKE) $(LEVELDB_TARGET_FLAGS) OPT="$(CXXFLAGS) $(CPPFLAGS)" libmemenv.a

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb Makefile.include

clean-local:
	-$(MAKE) -C leveldb clean
	rm -f leveldb/port/*.gcno leveldb/db/*.gcno leveldb/table/*.gcno leveldb/helpers/*.gcno
	rm -f leveldb/util/*.gcno leveldb/helpers/memenv/*.gcno
