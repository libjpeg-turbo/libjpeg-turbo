#!/bin/bash
# Generate diverse JPEG seed corpus for fuzzing
# This script creates various JPEG variants to maximize code coverage

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
TESTIMAGES="${SCRIPT_DIR}/../testimages"
CORPUS_DIR="${SCRIPT_DIR}/corpus"

CJPEG="${BUILD_DIR}/cjpeg-static"
DJPEG="${BUILD_DIR}/djpeg-static"
JPEGTRAN="${BUILD_DIR}/jpegtran-static"

# Check tools exist
if [ ! -x "$CJPEG" ]; then
    echo "Error: cjpeg-static not found at $CJPEG"
    echo "Build with: make cjpeg-static djpeg-static jpegtran-static"
    exit 1
fi

# Create corpus directories
mkdir -p "$CORPUS_DIR"/{decompress,compress,progressive,marker,coefficient,partial,error_recovery,raw_data,transcode,resync}

echo "=== Generating JPEG seed corpus ==="

# Source image (color)
SRC_PPM="${TESTIMAGES}/testorig.ppm"
# Source image (grayscale)
SRC_PGM="${TESTIMAGES}/testorig.pgm"
# 12-bit source
SRC_PPM16="${TESTIMAGES}/monkey16.ppm"

# ============================================
# 1. Baseline JPEGs with various quality levels
# ============================================
echo "Generating baseline JPEGs..."
for quality in 10 25 50 75 90 95 100; do
    "$CJPEG" -quality $quality -outfile "$CORPUS_DIR/decompress/baseline_q${quality}.jpg" "$SRC_PPM"
done

# ============================================
# 2. Different sampling factors (subsampling)
# ============================================
echo "Generating various sampling factors..."
for sample in "1x1,1x1,1x1" "2x1,1x1,1x1" "1x2,1x1,1x1" "2x2,1x1,1x1" "4x1,1x1,1x1" "1x4,1x1,1x1" "4x2,1x1,1x1" "2x4,1x1,1x1"; do
    name=$(echo "$sample" | tr ',' '_' | tr 'x' 'x')
    "$CJPEG" -quality 75 -sample "$sample" -outfile "$CORPUS_DIR/decompress/sample_${name}.jpg" "$SRC_PPM" 2>/dev/null || true
done

# ============================================
# 3. Grayscale JPEGs
# ============================================
echo "Generating grayscale JPEGs..."
"$CJPEG" -quality 75 -grayscale -outfile "$CORPUS_DIR/decompress/grayscale_q75.jpg" "$SRC_PPM"
"$CJPEG" -quality 90 -outfile "$CORPUS_DIR/decompress/grayscale_native_q90.jpg" "$SRC_PGM"

# ============================================
# 4. Progressive JPEGs
# ============================================
echo "Generating progressive JPEGs..."
"$CJPEG" -quality 75 -progressive -outfile "$CORPUS_DIR/progressive/prog_q75.jpg" "$SRC_PPM"
"$CJPEG" -quality 90 -progressive -outfile "$CORPUS_DIR/progressive/prog_q90.jpg" "$SRC_PPM"
"$CJPEG" -quality 50 -progressive -outfile "$CORPUS_DIR/progressive/prog_q50.jpg" "$SRC_PPM"

# Progressive with custom scan script (if available)
if [ -f "${TESTIMAGES}/test.scan" ]; then
    "$CJPEG" -quality 75 -scans "${TESTIMAGES}/test.scan" -outfile "$CORPUS_DIR/progressive/prog_custom_scan.jpg" "$SRC_PPM" 2>/dev/null || true
fi

# ============================================
# 5. Optimized Huffman tables
# ============================================
echo "Generating optimized Huffman JPEGs..."
"$CJPEG" -quality 75 -optimize -outfile "$CORPUS_DIR/decompress/optimized_q75.jpg" "$SRC_PPM"
"$CJPEG" -quality 90 -optimize -progressive -outfile "$CORPUS_DIR/progressive/prog_opt_q90.jpg" "$SRC_PPM"

# ============================================
# 6. Restart markers
# ============================================
echo "Generating JPEGs with restart markers..."
for restart in 1 2 4 8 16 32 64 100; do
    "$CJPEG" -quality 75 -restart $restart -outfile "$CORPUS_DIR/resync/restart_${restart}.jpg" "$SRC_PPM"
done

# Restart with progressive
"$CJPEG" -quality 75 -progressive -restart 8 -outfile "$CORPUS_DIR/resync/prog_restart_8.jpg" "$SRC_PPM"

# ============================================
# 7. Arithmetic coding (if supported)
# ============================================
echo "Generating arithmetic coding JPEGs..."
"$CJPEG" -quality 75 -arithmetic -outfile "$CORPUS_DIR/decompress/arithmetic_q75.jpg" "$SRC_PPM" 2>/dev/null || echo "  Arithmetic coding not supported or failed"
"$CJPEG" -quality 90 -arithmetic -progressive -outfile "$CORPUS_DIR/progressive/prog_arith_q90.jpg" "$SRC_PPM" 2>/dev/null || echo "  Progressive arithmetic not supported or failed"

# ============================================
# 8. DCT methods
# ============================================
echo "Generating JPEGs with different DCT methods..."
for dct in int fast float; do
    "$CJPEG" -quality 75 -dct $dct -outfile "$CORPUS_DIR/decompress/dct_${dct}_q75.jpg" "$SRC_PPM"
done

# ============================================
# 9. Small images (edge cases)
# ============================================
echo "Generating small test images..."
# Create tiny PPM images
echo -e "P6\n1 1\n255\n\xff\x00\x00" > /tmp/1x1.ppm
echo -e "P6\n2 2\n255\n\xff\x00\x00\x00\xff\x00\x00\x00\xff\xff\xff\x00" > /tmp/2x2.ppm
echo -e "P6\n8 8\n255" > /tmp/8x8.ppm
for i in $(seq 1 192); do printf '\xff\x80\x00'; done >> /tmp/8x8.ppm

"$CJPEG" -quality 75 -outfile "$CORPUS_DIR/decompress/tiny_1x1.jpg" /tmp/1x1.ppm 2>/dev/null || true
"$CJPEG" -quality 75 -outfile "$CORPUS_DIR/decompress/tiny_2x2.jpg" /tmp/2x2.ppm 2>/dev/null || true
"$CJPEG" -quality 75 -outfile "$CORPUS_DIR/decompress/tiny_8x8.jpg" /tmp/8x8.ppm 2>/dev/null || true
"$CJPEG" -quality 75 -sample "2x2,1x1,1x1" -outfile "$CORPUS_DIR/decompress/tiny_8x8_420.jpg" /tmp/8x8.ppm 2>/dev/null || true

rm -f /tmp/1x1.ppm /tmp/2x2.ppm /tmp/8x8.ppm

# ============================================
# 10. Transformed JPEGs (via jpegtran)
# ============================================
echo "Generating transformed JPEGs..."
BASE_JPG="$CORPUS_DIR/decompress/baseline_q75.jpg"

# Various transformations
"$JPEGTRAN" -rotate 90 -outfile "$CORPUS_DIR/transcode/rotate90.jpg" "$BASE_JPG"
"$JPEGTRAN" -rotate 180 -outfile "$CORPUS_DIR/transcode/rotate180.jpg" "$BASE_JPG"
"$JPEGTRAN" -rotate 270 -outfile "$CORPUS_DIR/transcode/rotate270.jpg" "$BASE_JPG"
"$JPEGTRAN" -flip horizontal -outfile "$CORPUS_DIR/transcode/flip_h.jpg" "$BASE_JPG"
"$JPEGTRAN" -flip vertical -outfile "$CORPUS_DIR/transcode/flip_v.jpg" "$BASE_JPG"
"$JPEGTRAN" -transpose -outfile "$CORPUS_DIR/transcode/transpose.jpg" "$BASE_JPG"
"$JPEGTRAN" -transverse -outfile "$CORPUS_DIR/transcode/transverse.jpg" "$BASE_JPG"

# Crop operations
"$JPEGTRAN" -crop 16x16+0+0 -outfile "$CORPUS_DIR/partial/crop_16x16.jpg" "$BASE_JPG" 2>/dev/null || true
"$JPEGTRAN" -crop 32x32+8+8 -outfile "$CORPUS_DIR/partial/crop_32x32_offset.jpg" "$BASE_JPG" 2>/dev/null || true

# Progressive conversion
"$JPEGTRAN" -progressive -outfile "$CORPUS_DIR/progressive/converted_prog.jpg" "$BASE_JPG"

# Optimize existing
"$JPEGTRAN" -optimize -outfile "$CORPUS_DIR/transcode/reoptimized.jpg" "$BASE_JPG"

# ============================================
# 11. JPEGs with markers
# ============================================
echo "Generating JPEGs with various markers..."
# Add comment marker
"$CJPEG" -quality 75 -outfile "$CORPUS_DIR/marker/with_comment.jpg" "$SRC_PPM"
# wrjpgcom would add comments but we use what we have

# Copy existing test images that have special properties
cp "${TESTIMAGES}/testimgari.jpg" "$CORPUS_DIR/decompress/original_arithmetic.jpg" 2>/dev/null || true
cp "${TESTIMAGES}/testimgint.jpg" "$CORPUS_DIR/decompress/original_int.jpg" 2>/dev/null || true

# ============================================
# 12. ICC Profile handling
# ============================================
echo "Generating JPEGs with ICC profiles..."
if [ -f "${TESTIMAGES}/test1.icc" ]; then
    "$CJPEG" -quality 75 -icc "${TESTIMAGES}/test1.icc" -outfile "$CORPUS_DIR/marker/with_icc_large.jpg" "$SRC_PPM" 2>/dev/null || echo "  ICC profile embedding failed"
fi
if [ -f "${TESTIMAGES}/test3.icc" ]; then
    "$CJPEG" -quality 75 -icc "${TESTIMAGES}/test3.icc" -outfile "$CORPUS_DIR/marker/with_icc_small.jpg" "$SRC_PPM" 2>/dev/null || echo "  ICC profile embedding failed"
fi

# ============================================
# 13. 12-bit precision (if source available)
# ============================================
echo "Generating 12-bit JPEGs..."
if [ -f "$SRC_PPM16" ]; then
    "$CJPEG" -quality 75 -outfile "$CORPUS_DIR/decompress/12bit_q75.jpg" "$SRC_PPM16" 2>/dev/null || echo "  12-bit encoding failed"
fi

# ============================================
# 14. Raw YUV data samples (for raw_data fuzzer)
# ============================================
echo "Generating samples for raw data fuzzer..."
for sample in "1x1" "2x1" "1x2" "2x2"; do
    h=$(echo "$sample" | cut -dx -f1)
    v=$(echo "$sample" | cut -dx -f2)
    "$CJPEG" -quality 75 -sample "${h}x${v},1x1,1x1" -outfile "$CORPUS_DIR/raw_data/yuv_${sample}.jpg" "$SRC_PPM" 2>/dev/null || true
done

# ============================================
# 15. Copy base samples to other corpus dirs
# ============================================
echo "Distributing samples to specialized corpus directories..."

# Coefficient fuzzer needs valid JPEGs
cp "$CORPUS_DIR/decompress/baseline_q75.jpg" "$CORPUS_DIR/coefficient/"
cp "$CORPUS_DIR/progressive/prog_q75.jpg" "$CORPUS_DIR/coefficient/"

# Error recovery needs various samples
cp "$CORPUS_DIR/decompress/baseline_q75.jpg" "$CORPUS_DIR/error_recovery/"
cp "$CORPUS_DIR/progressive/prog_q75.jpg" "$CORPUS_DIR/error_recovery/"
cp "$CORPUS_DIR/resync/restart_8.jpg" "$CORPUS_DIR/error_recovery/"

# Partial decoder
cp "$CORPUS_DIR/decompress/baseline_q75.jpg" "$CORPUS_DIR/partial/"
cp "$CORPUS_DIR/decompress/sample_2x2_1x1_1x1.jpg" "$CORPUS_DIR/partial/" 2>/dev/null || true

# Marker fuzzer
cp "$CORPUS_DIR/decompress/baseline_q75.jpg" "$CORPUS_DIR/marker/"
cp "$CORPUS_DIR/progressive/prog_q75.jpg" "$CORPUS_DIR/marker/"

# Resync fuzzer
cp "$CORPUS_DIR/decompress/baseline_q75.jpg" "$CORPUS_DIR/resync/"

# Transcode fuzzer
cp "$CORPUS_DIR/decompress/baseline_q75.jpg" "$CORPUS_DIR/transcode/"
cp "$CORPUS_DIR/progressive/prog_q75.jpg" "$CORPUS_DIR/transcode/"

# Compress corpus uses PPM files, copy existing test images
mkdir -p "$CORPUS_DIR/compress"
cp "$SRC_PPM" "$CORPUS_DIR/compress/testorig.ppm" 2>/dev/null || true
cp "$SRC_PGM" "$CORPUS_DIR/compress/testorig.pgm" 2>/dev/null || true

# ============================================
# Summary
# ============================================
echo ""
echo "=== Corpus generation complete ==="
echo ""
find "$CORPUS_DIR" -type f -name "*.jpg" | wc -l | xargs echo "Total JPEG files:"
echo ""
echo "Corpus directories:"
for dir in "$CORPUS_DIR"/*/; do
    count=$(find "$dir" -type f 2>/dev/null | wc -l)
    echo "  $(basename "$dir"): $count files"
done
