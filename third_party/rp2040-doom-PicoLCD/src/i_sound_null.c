// Silent sound backend for display-only RP2040 builds.

#include "i_sound.h"
#include "pico/i_picosound.h"

#include <stddef.h>
#include <stdint.h>

int snd_samplerate = 0;
int snd_cachesize = 0;
int snd_maxslicetime_ms = 0;
should_be_const constcharstar snd_musiccmd = "";
isb_int8_t snd_pitchshift = 0;
uint8_t restart_song_state = 0;

void I_InitSound(boolean use_sfx_prefix) { (void)use_sfx_prefix; }
void I_ShutdownSound(void) {}
int I_GetSfxLumpNum(should_be_const sfxinfo_t *sfxinfo) { (void)sfxinfo; return 0; }
void I_UpdateSound(void) {}
void I_UpdateSoundParams(int channel, int vol, int sep)
{
    (void)channel; (void)vol; (void)sep;
}
int I_StartSound(should_be_const sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch)
{
    (void)sfxinfo; (void)channel; (void)vol; (void)sep; (void)pitch;
    return -1;
}
void I_StopSound(int channel) { (void)channel; }
boolean I_SoundIsPlaying(int channel) { (void)channel; return false; }
void I_PrecacheSounds(should_be_const sfxinfo_t *sounds, int num_sounds)
{
    (void)sounds; (void)num_sounds;
}
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_SetMusicVolume(int volume) { (void)volume; }
void I_PauseSong(void) {}
void I_ResumeSong(void) {}
void *I_RegisterSong(should_be_const void *data, int len)
{
    (void)data; (void)len; return NULL;
}
void I_UnRegisterSong(void *handle) { (void)handle; }
void I_PlaySong(void *handle, boolean looping) { (void)handle; (void)looping; }
void I_StopSong(void) {}
boolean I_MusicIsPlaying(void) { return false; }
void I_BindSoundVariables(void) {}
void I_SetOPLDriverVer(opl_driver_ver_t version) { (void)version; }
void I_PicoSoundSetMusicGenerator(void (*generator)(audio_buffer_t *buffer)) { (void)generator; }
bool I_PicoSoundIsInitialized(void) { return false; }
void I_PicoSoundFade(bool in) { (void)in; }
bool I_PicoSoundFading(void) { return false; }
