set(QW_MP3_DECODER_SRCS
    "src/bitstream.c"
    "src/buffers.c"
    "src/dct32.c"
    "src/dequant.c"
    "src/dqchan.c"
    "src/huffman.c"
    "src/hufftabs.c"
    "src/imdct.c"
    "src/mp3dec.c"
    "src/mp3tabs.c"
    "src/polyphase.c"
    "src/scalfact.c"
    "src/stproc.c"
    "src/subband.c"
    "src/trigtabs.c")

idf_component_register(SRCS ${QW_MP3_DECODER_SRCS}
                       INCLUDE_DIRS "include")

# The imported Helix implementation uses this define to select its portable
# GCC fixed-point arithmetic path. It does not enable ARM assembly.
target_compile_definitions(${COMPONENT_LIB} PRIVATE ARM)
target_compile_options(${COMPONENT_LIB} PRIVATE
                       -Wno-unused-but-set-variable)
