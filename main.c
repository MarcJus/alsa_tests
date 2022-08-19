#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <alloca.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

#define SAMPLE_RATE 48000

#define CHECK_ALSA(value, message, action) if(value < 0){fprintf(stderr, message, snd_strerror(value));action}

typedef unsigned char u8;
typedef short s16;
typedef float f32;

#define CHECK(value, action) if(value < 0){action;}

short *sin_wave(short *buffer, size_t sample_count, int freq){
    for(int i = 0; i < sample_count; i++){
        buffer[i] = 10000 * sinf(2 * M_PI * freq * ((float)i / SAMPLE_RATE));
    }
    return buffer;
}

short *square_wave(short *buffer, size_t sample_count, int freq) {
    int samples_full_cycle = (float)SAMPLE_RATE / (float)freq;
    int samples_half_cycle = samples_full_cycle / 2.0f;
    int cycle_index = 0;
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = cycle_index < samples_half_cycle ? 10000 : -10000;
        cycle_index = (cycle_index + 1) % samples_full_cycle;
    }
    return buffer;
}

f32 clamp(f32 value, f32 min, f32 max){
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

int main() {

    int rc;
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *params;
    uint val, val2;

    rc = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    CHECK_ALSA(rc, "cannot open device : %s\n", ;)

    // Définition des paramètres
    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(pcm, params);

    snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_MMAP_INTERLEAVED);

    snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);

    snd_pcm_hw_params_set_channels(pcm, params, 1);

    snd_pcm_hw_params_set_rate(pcm, params, SAMPLE_RATE, 0);

    snd_pcm_hw_params_set_periods(pcm, params, 10, 0);

    snd_pcm_hw_params_set_period_time(pcm, params, 100000, 0);

    snd_pcm_hw_params(pcm, params);

    int fd = open("/home/marc/CLionProjects/alsa_tests/nightcall.raw", O_RDWR);
    CHECK(fd, perror("Impossible d'ouvrir le fichier"); snd_pcm_close(pcm); exit(EXIT_FAILURE);)

    ssize_t ret = 0;

    struct stat stat;
    fstat(fd, &stat);
    size_t file_size = stat.st_size; // récupération de la taille du fichier

//    errno = 0;
//    char *file = malloc(file_size);
//    ret = read(fd, file, file_size);
//    CHECK(ret, perror("Erreur read"); close(fd); snd_pcm_close(pcm); free(file); exit(EXIT_FAILURE);)
//    s16 *samples = (s16*) file;

    int sample_index = 0;
    for (int offset = 0; offset < file_size; offset += SAMPLE_RATE * sizeof(s16)) { // on prend chaque seconde : 1 sec = 48000 short = SAMPLE_RATE * sizeof(s16)
        int chunk_sample_count = SAMPLE_RATE;
        int chunk_size = chunk_sample_count * sizeof(s16); // nombre d'octets contenus dans une seconde d'audio
        if (offset + chunk_size > file_size) {
            chunk_sample_count = (file_size - offset) / sizeof(s16);
        }

        s16 *sample_to_write = malloc(chunk_size); // tableau qui contiendra les données qui représentent 1 seconde d'audio
//        sample_to_write = (s16 *) ((u8 *) samples + offset);
        ret = read(fd, sample_to_write, chunk_size);
        CHECK(ret, perror("Erreur read"); free(sample_to_write); break;)
        for (int i = 0; i < chunk_sample_count; i++) { // on prend chaque échantillion
            f32 volume = 1.0f;
            s16 *sample = sample_to_write + i;
            f32 normalized_sample = *sample / (f32) 32768;
            normalized_sample *= volume;
            *sample = (s16) clamp(normalized_sample * 32768, -32768, 32767);

            sample_index++;
        }
        snd_pcm_writei(pcm, /*(u8*)samples + offset*/ sample_to_write, chunk_sample_count); // on écrit les données pour 1 seconde d'audio
        free(sample_to_write);
    }
    close(fd);
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);

    return 0;
}
