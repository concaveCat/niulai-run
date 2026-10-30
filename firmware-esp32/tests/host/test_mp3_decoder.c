#include "qw_check.h"
#include "mp3dec.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define QW_MP3_ID3_HEADER_BYTES 10U
#define QW_MP3_MAX_PCM_SAMPLES (MAX_NGRAN * MAX_NSAMP * MAX_NCHAN)

static size_t skip_id3v2(const uint8_t *data, size_t length)
{
    if (length < QW_MP3_ID3_HEADER_BYTES || data[0] != 'I' ||
        data[1] != 'D' || data[2] != '3') {
        return 0U;
    }

    const size_t tag_size = ((size_t)(data[6] & 0x7fU) << 21U) |
                            ((size_t)(data[7] & 0x7fU) << 14U) |
                            ((size_t)(data[8] & 0x7fU) << 7U) |
                            (size_t)(data[9] & 0x7fU);
    size_t total = QW_MP3_ID3_HEADER_BYTES + tag_size;
    if ((data[5] & 0x10U) != 0U) {
        total += QW_MP3_ID3_HEADER_BYTES;
    }
    return total <= length ? total : length;
}

int main(int argc, char **argv)
{
    CHECK_INT_EQ(argc, 2);
    FILE *file = fopen(argv[1], "rb");
    CHECK(file != NULL);
    CHECK(fseek(file, 0L, SEEK_END) == 0);
    const long file_size = ftell(file);
    CHECK(file_size > 0L);
    CHECK(fseek(file, 0L, SEEK_SET) == 0);

    uint8_t *data = (uint8_t *)malloc((size_t)file_size);
    CHECK(data != NULL);
    CHECK(fread(data, 1U, (size_t)file_size, file) == (size_t)file_size);
    CHECK(fclose(file) == 0);

    const size_t offset = skip_id3v2(data, (size_t)file_size);
    CHECK(offset < (size_t)file_size);
    HMP3Decoder decoder = MP3InitDecoder();
    CHECK(decoder != NULL);

    unsigned char *cursor = data + offset;
    int bytes_left = (int)((size_t)file_size - offset);
    const int first_sync = MP3FindSyncWord(cursor, bytes_left);
    CHECK(first_sync >= 0);
    cursor += first_sync;
    bytes_left -= first_sync;

    MP3FrameInfo first_frame;
    CHECK_INT_EQ(MP3GetNextFrameInfo(decoder, &first_frame, cursor),
                 ERR_MP3_NONE);
    CHECK_INT_EQ(first_frame.layer, 3);
    CHECK_INT_EQ(first_frame.samprate, 44100);
    CHECK_INT_EQ(first_frame.nChans, 2);

    short pcm[QW_MP3_MAX_PCM_SAMPLES];
    unsigned frame_count = 0U;
    size_t decoded_samples = 0U;
    while (bytes_left > 0) {
        const int sync = MP3FindSyncWord(cursor, bytes_left);
        if (sync < 0) {
            break;
        }
        cursor += sync;
        bytes_left -= sync;
        const int bytes_before_decode = bytes_left;
        const int decode_error = MP3Decode(decoder, &cursor, &bytes_left,
                                           pcm, 0);
        CHECK_INT_EQ(decode_error, ERR_MP3_NONE);
        CHECK(bytes_left < bytes_before_decode);

        MP3FrameInfo frame;
        MP3GetLastFrameInfo(decoder, &frame);
        CHECK_INT_EQ(frame.layer, 3);
        CHECK_INT_EQ(frame.samprate, 44100);
        CHECK_INT_EQ(frame.nChans, 2);
        CHECK(frame.outputSamps > 0 &&
              frame.outputSamps <= QW_MP3_MAX_PCM_SAMPLES);
        decoded_samples += (size_t)frame.outputSamps;
        ++frame_count;
    }

    MP3FreeDecoder(decoder);
    free(data);
    CHECK_INT_EQ(frame_count, 169);
    CHECK(decoded_samples > 380000U && decoded_samples < 400000U);
    printf("test_mp3_decoder: frames=%u samples=%u rate=44100 channels=2\n",
           frame_count, (unsigned)decoded_samples);
    printf("test_mp3_decoder: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
