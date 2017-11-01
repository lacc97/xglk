#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xglk.h"
#include "xg_internal.h"

/* Sound support using SDL_mixer. AIFF support in SDL_mixer 1.2.0 does not
 * work, so we're going to need something more recent than that. Even then
 * samples may come out at slightly the wrong speed because SDL needs to
 * convert all sound data to a common frequency and it does so favoring speed
 * over accuracy.
 */

#ifdef GLK_MODULE_SOUND

#define DEBUG_MODULE_SOUND 1

#include "gi_blorb.h"

void music_finished_hook(void);

#define giblorb_ID_MOD	(giblorb_make_id('M', 'O', 'D', ' '))
#define giblorb_ID_FORM	(giblorb_make_id('F', 'O', 'R', 'M'))
#define giblorb_ID_SONG (giblorb_make_id('S', 'O', 'N', 'G'))

static channel_t *gli_channellist = NULL;
static Bool music_playing = FALSE;

static void clear_channel(schanid_t chan)
{
  if (!chan)
    return;

  if (chan->filename[0] != '\0') {
    unlink(chan->filename);
    chan->filename[0] = '\0';
  }

  if (chan->type == SCHANNEL_MUSIC && chan->data.music != NULL) {
    if (Mix_PlayingMusic())
      Mix_HaltMusic();
    Mix_FreeMusic(chan->data.music);
    chan->data.music = NULL;
    music_playing = FALSE;
  }

  if (chan->type == SCHANNEL_SAMPLE && chan->data.sample != NULL) {
    if (Mix_Playing(chan->channel))
      Mix_HaltChannel(chan->channel);
    Mix_FreeChunk(chan->data.sample);
    chan->data.sample = NULL;
  }
}

static void set_channel_volume(schanid_t chan, glui32 vol)
{
  if (!chan)
    return;

  if (chan->type == SCHANNEL_MUSIC && chan->data.music)
    Mix_VolumeMusic((MIX_MAX_VOLUME * vol) / 0x10000);

  if (chan->type == SCHANNEL_SAMPLE && chan->data.sample)
    Mix_Volume(chan->channel, (MIX_MAX_VOLUME * vol) / 0x10000);
}

void music_finished_hook()
{
  music_playing = FALSE;
#ifdef DEBUG_MODULE_SOUND
  fprintf(stderr, "music_finished_hook()\n");
#endif
}

Bool gli_eventloop_schannels()
{
  channel_t *chan = gli_channellist;

  while (chan) {
    switch (chan->type) {
      case SCHANNEL_MUSIC:
	/* We cannot use Mix_PlayingMusic() to see if the music is playing
	 * because that will break repeating music.
	 */
	if (chan->data.music != NULL && !music_playing) {
#if DEBUG_MODULE_SOUND
	  fprintf(stderr, "Sound %ld (music) has stopped: notify %ld\n",
		 chan->snd, chan->notify);
#endif
	  clear_channel(chan);
	  if (chan->notify != 0) {
	    eventloop_setevent(evtype_SoundNotify, NULL, chan->snd, chan->notify);
	    chan->notify = 0;
	    return TRUE;
	  }
	}
	break;

      case SCHANNEL_SAMPLE:
	if (chan->data.sample != NULL && !Mix_Playing(chan->channel)) {
#if DEBUG_MODULE_SOUND
	  fprintf(stderr, "Sound %ld (sample) has stopped: notify %ld\n",
		 chan->snd, chan->notify);
#endif
	  clear_channel(chan);
	  if (chan->notify != 0) {
	    eventloop_setevent(evtype_SoundNotify, NULL, chan->snd, chan->notify);
	    chan->notify = 0;
	    return TRUE;
	  }
	}
	break;

      default:
	break;
    }
    chan = chan->chain_next;
  }
  return FALSE;
}

int init_gli_schannels()
{
  int    audio_rate     = 22050;
  Uint16 audio_format   = AUDIO_S16;
  int    audio_channels = 2;
  int    audio_buffers  = 4096;

  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
    return FALSE;
  }

  if (Mix_OpenAudio(audio_rate, audio_format, audio_channels,
		    audio_buffers) < 0) {
    fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
    return FALSE;
  }

  /* As far as I understand, we will get one music channels and eight AIFF
   * channels. We can't do anything about the number of music channels, but
   * we could allocate more AIFF channels with Mix_AllocateChannels()
   */

#if DEBUG_MODULE_SOUND
  Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
  fprintf(stderr, "Opened audio at %d Hz %d bit %s\n", audio_rate,
	(audio_format & 0xff),
	(audio_channels > 1) ? "stereo" : "mono");
#endif

  /* Notify when the music has stopped playing */
  Mix_HookMusicFinished(music_finished_hook);

  /* Set the external music player, if any */
  Mix_SetMusicCMD(getenv("MUSIC_CMD"));

  return TRUE;
}

void exit_gli_schannels()
{
  channel_t *chan = gli_channellist;

  while (chan) {
    clear_channel(chan);
    chan = chan->chain_next;
  }

  Mix_CloseAudio();
}

schanid_t glk_schannel_create(glui32 rock)
{
  channel_t *chan = (channel_t *)malloc(sizeof(channel_t));

  if (!chan)
  return NULL;

  chan->rock = rock;
  chan->vol = 0x10000;

  chan->chain_prev = NULL;
  chan->chain_next = gli_channellist;
  gli_channellist = chan;
  if (chan->chain_next) {
    chan->chain_next->chain_prev = chan;
  }

  if (gli_register_obj)
    chan->disprock = (*gli_register_obj)(chan, gidisp_Class_Schannel);
  else
    chan->disprock.ptr = NULL;

  return chan;  
}

void glk_schannel_destroy(schanid_t chan)
{
  channel_t *prev, *next;

  if (!chan) {
    gli_strict_warning("schannel_destroy: invalid id.");
    return;
  }

  if (gli_unregister_obj)
    (*gli_unregister_obj)(chan, gidisp_Class_Schannel, chan->disprock);

  prev = chan->chain_prev;
  next = chan->chain_next;
  chan->chain_prev = NULL;
  chan->chain_next = NULL;

  if (prev)
    prev->chain_next = next;
  else
    gli_channellist = next;
  if (next)
    next->chain_prev = prev;

  clear_channel(chan);
  free(chan);
}

channel_t *glk_schannel_iterate(schanid_t chan, glui32 *rock)
{
  if (!chan) {
    chan = gli_channellist;
  } else {
    chan = chan->chain_next;
  }

  if (chan) {
    if (rock)
      *rock = chan->rock;
    return chan;
  }

  if (rock)
    *rock = 0;
  return NULL;
}

glui32 glk_schannel_get_rock(channel_t *chan)
{
  if (!chan) {
  gli_strict_warning("schannel_get_rock: invalid id.");
  return 0;
  }
  return chan->rock;
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
  /* Error messages will be slightly wrong, but I'm lazy... */
  return glk_schannel_play_ext(chan, snd, 1, 0);
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats,
    glui32 notify)
{
  FILE *fl, *tmpfl;
  long pos, remaining, expected, read_len;
  glui32 chunktype;
  unsigned char filebuf[BUFSIZ];

#if DEBUG_MODULE_SOUND
  fprintf(stderr, "play snd %ld (notify %ld, loop %ld)\n",
	snd, notify, repeats);
#endif

  if (!chan) {
  gli_strict_warning("schannel_play_ext: invalid id.");
  return 0;
  }

  clear_channel(chan);

  if (repeats == 0)
    return 1;

  /* TODO: Allow picture_find()-style reading (and caching?) */
  if (!xres_is_resource_map()) {
    gli_strict_warning("schannel_play_ext: no resource map.");
    return 0;
  }

  xres_get_resource(giblorb_ID_Snd, snd, &fl, &pos, &remaining, &chunktype);

  if (!fl) {
    gli_strict_warning("schannel_play_ext: internal error - no file pointer.");
    return 0;
  }

  fseek(fl, pos, 0);

  /* TODO: Better temp-file handling. This is supposedly not quite secure. */
  if (tmpnam(chan->filename) == NULL) {
    gli_strict_warning("schannel_play_ext: tmpnam failed.");
    return 0;
  }

  tmpfl = fopen(chan->filename, "wb");
  if (tmpfl == NULL) {
    gli_strict_warning("schannel_play_ext: could not create temporary file.");
    return 0;
  }

  while (remaining > 0) {
    expected = (remaining < BUFSIZ) ? remaining : BUFSIZ;
    read_len = fread(filebuf, 1, expected, fl);
    
    if (read_len != expected) {
      gli_strict_warning("schannel_play_ext: unexpected end-of-file.");
      remaining = 0;
    }

    fwrite(filebuf, 1, read_len, tmpfl);
    remaining -= read_len;
  }

  fclose(tmpfl);

  chan->snd = snd;
  chan->notify = notify;

  switch (chunktype) {
    case giblorb_ID_MOD:
      if (Mix_PlayingMusic()) {
	gli_strict_warning("schannel_play_ext: music is already playing.");
	unlink(chan->filename);
	return 0;
      }

      chan->type = SCHANNEL_MUSIC;
      chan->data.music = Mix_LoadMUS(chan->filename);
      if (chan->data.music == NULL) {
	gli_strict_warning("schannel_play_ext: Mix_LoadMUS() failure.");
	return 0;
      }
      if (Mix_PlayMusic(chan->data.music, repeats) == -1) {
	gli_strict_warning("schannel_play_ext: Mix_PlayMusic() failure.");
	Mix_FreeMusic(chan->data.music);
	chan->data.music = NULL;
	return 0;
      }
      music_playing = TRUE;
      break;

    case giblorb_ID_FORM:
      chan->type = SCHANNEL_SAMPLE;
      chan->data.sample = Mix_LoadWAV(chan->filename);
      if (chan->data.sample == NULL) {
	gli_strict_warning("schannel_play_ext: Mix_LoadWAV() failure.");
	return 0;
      }
      chan->channel = Mix_PlayChannel(-1, chan->data.sample,
		(repeats == -1) ? -1 : repeats - 1);
      if (chan->channel == -1) {
	gli_strict_warning("schannel_play_ext: Mix_PlayChannel() failure.");
	Mix_FreeChunk(chan->data.sample);
	chan->data.sample = NULL;
	return 0;
      }
      break;

    case giblorb_ID_SONG:
      gli_strict_warning("schannel_play_ext: sound type 'SONG' is not supported.");
      unlink(chan->filename);
      return 0;
  }

  set_channel_volume(chan, chan->vol);
  return 1;
}

void glk_schannel_stop(schanid_t chan)
{
  if (!chan) {
  gli_strict_warning("schannel_stop: invalid id.");
    return;
  }

  clear_channel(chan);
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
  if (!chan) {
  gli_strict_warning("schannel_set_volume: invalid id.");
    return;
  }

  chan->vol = vol;
  set_channel_volume(chan, vol);
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
  /* I doubt this will make much difference, so make it a no-op for now. */
}

#endif /* GLK_MODULE_SOUND */
