#!/bin/bash

set -u
set -e

FUZZER_SUFFIX=
if [ $# -ge 1 ]; then
	FUZZER_SUFFIX="$1"
	FUZZER_SUFFIX="`echo $1 | sed 's/\./_/g'`"
fi

# OSS-Fuzz provides these environment variables:
# CC, CXX, CFLAGS, CXXFLAGS, LIB_FUZZING_ENGINE, OUT, SRC, WORK, SANITIZER
#
# Supported sanitizers (via $SANITIZER):
#   - address: AddressSanitizer (ASan) - heap/stack buffer overflows, use-after-free
#   - memory: MemorySanitizer (MSan) - uninitialized memory reads
#   - undefined: UndefinedBehaviorSanitizer (UBSan) - integer overflow, null deref, etc.
#   - coverage: Coverage instrumentation for corpus generation

if [ "$SANITIZER" = "memory" ]; then
	# MSan requires zero-initialized buffers to avoid false positives
	export CFLAGS="$CFLAGS -DZERO_BUFFERS=1"
fi

# UBSan: Sanitizer flags are passed through $CFLAGS/$CXXFLAGS from OSS-Fuzz.
# OSS-Fuzz automatically sets silence_unsigned_overflow=1 since unsigned
# overflow is not undefined behavior.

# Use OSS-Fuzz provided compilers and flags
# Note: Use $CXX as linker for C++ targets (OSS-Fuzz requirement)
cmake . -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_STATIC=1 -DENABLE_SHARED=0 \
	-DCMAKE_C_COMPILER="$CC" \
	-DCMAKE_CXX_COMPILER="$CXX" \
	-DCMAKE_C_FLAGS="$CFLAGS" \
	-DCMAKE_CXX_FLAGS="$CXXFLAGS" \
	-DCMAKE_C_FLAGS_RELWITHDEBINFO="-g -DNDEBUG" \
	-DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-g -DNDEBUG" \
	-DCMAKE_INSTALL_PREFIX=$WORK \
	-DWITH_FUZZ=1 -DFUZZ_BINDIR=$OUT -DFUZZ_LIBRARY=$LIB_FUZZING_ENGINE \
	-DFUZZER_SUFFIX="$FUZZER_SUFFIX"
make "-j$(nproc)" "--load-average=$(nproc)"
make install

# ============================================
# Copy JPEG dictionary and options for all fuzzers
# ============================================
FUZZ_DIR="$(dirname "$0")"

# List of all fuzzers
FUZZERS="cjpeg compress compress_yuv compress_lossless compress12 \
         compress12_lossless compress16_lossless libjpeg_turbo \
         decompress_yuv transform decompress_libjpeg progressive \
         marker coefficient partial error_recovery raw_data \
         transcode compress_libjpeg compress_raw_data resync"

# Copy dictionary for each fuzzer (libFuzzer uses fuzzer_name.dict convention)
if [ -f "$FUZZ_DIR/jpeg.dict" ]; then
	for fuzzer in $FUZZERS; do
		cp "$FUZZ_DIR/jpeg.dict" "$OUT/${fuzzer}_fuzzer${FUZZER_SUFFIX}.dict"
	done
fi

# Copy options file for each fuzzer (sets max_len, timeout, rss_limit_mb)
if [ -f "$FUZZ_DIR/fuzzer.options" ]; then
	for fuzzer in $FUZZERS; do
		cp "$FUZZ_DIR/fuzzer.options" "$OUT/${fuzzer}_fuzzer${FUZZER_SUFFIX}.options"
	done
fi

# ============================================
# Copy seed corpora (use specialized if available, fallback to generic)
# ============================================

# Helper function to copy corpus with fallback
copy_corpus() {
	local fuzzer_name="$1"
	local specialized_corpus="$2"
	local fallback_corpus="$3"

	if [ -f "$FUZZ_DIR/${specialized_corpus}_seed_corpus.zip" ]; then
		cp "$FUZZ_DIR/${specialized_corpus}_seed_corpus.zip" "$OUT/${fuzzer_name}_fuzzer${FUZZER_SUFFIX}_seed_corpus.zip"
	elif [ -f "$SRC/${fallback_corpus}_fuzzer_seed_corpus.zip" ]; then
		cp "$SRC/${fallback_corpus}_fuzzer_seed_corpus.zip" "$OUT/${fuzzer_name}_fuzzer${FUZZER_SUFFIX}_seed_corpus.zip"
	fi
}

# Compression fuzzers (use compress corpus or PPM-based samples)
copy_corpus "cjpeg" "compress" "compress"
copy_corpus "compress" "compress" "compress"
copy_corpus "compress_yuv" "compress" "compress"
copy_corpus "compress_lossless" "compress" "compress"
copy_corpus "compress12" "compress" "compress"
copy_corpus "compress12_lossless" "compress" "compress"
copy_corpus "compress16_lossless" "compress" "compress"
copy_corpus "compress_libjpeg" "compress" "compress"
copy_corpus "compress_raw_data" "raw_data" "compress"

# Decompression fuzzers (use specialized corpora)
copy_corpus "libjpeg_turbo" "decompress" "decompress"
copy_corpus "decompress_yuv" "decompress" "decompress"
copy_corpus "transform" "transcode" "decompress"
copy_corpus "decompress_libjpeg" "decompress" "decompress"

# Specialized fuzzers with dedicated corpora
copy_corpus "progressive" "progressive" "decompress"
copy_corpus "marker" "marker" "decompress"
copy_corpus "coefficient" "coefficient" "decompress"
copy_corpus "partial" "partial" "decompress"
copy_corpus "error_recovery" "error_recovery" "decompress"
copy_corpus "raw_data" "raw_data" "decompress"
copy_corpus "transcode" "transcode" "decompress"
copy_corpus "resync" "resync" "decompress"
