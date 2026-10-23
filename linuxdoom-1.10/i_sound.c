// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the GNU General Public License 2.0.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	System interface for sound, on SDL3.
//	Sound effects are mixed here, in the audio callback, together
//	with OPL music; this replaces the separate sndserver process.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL3/SDL.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_sound.h"
#include "i_oplmus.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"


// Options, saved in the config file.
int		snd_samplerate = 48000;
int		opl_type = 3;		// 2 for OPL2 (9 voices, mono), 3 for OPL3
int		snd_pitchshift = 0;	// v1.9 played every sound at its own pitch


// More than s_sound will ever ask for at once.
#define NUM_MIXCHANNELS		32

// Output gain of each part of the mix.
#define SFX_GAIN		0.9f
#define MUSIC_GAIN		1.6f

typedef struct
{
    const byte*	samples;	// 8-bit unsigned PCM
    int		length;		// in samples
    int		rate;		// samples per second
} sfxdata_t;

typedef struct
{
    const sfxdata_t*	sfx;
    int			sfxid;
    int			handle;		// 0 when the channel is free
    Uint64		pos;		// 32.32 fixed point sample position
    Uint64		step;		// per output frame
    float		leftgain;
    float		rightgain;
} mixchannel_t;


static SDL_AudioStream*	stream;
static int		outrate;

static sfxdata_t	sfxdata[NUMSFX];
static mixchannel_t	mixchannels[NUM_MIXCHANNELS];
static int		nexthandle = 1;

static boolean		nosfx;
static boolean		nomusic;
static boolean		musicready;

static float*		mixbuffer;
static short*		musicbuffer;
static int		mixbufferframes;

// Several OPL waveforms are positive only, so the chip's output sits
// well off zero; sound cards took that out with a capacitor. This is
// the same thing, a high-pass filter at about 10Hz.
static float		dcblockcoef;
static float		dcblockin[2];
static float		dcblockout[2];


//
// LoadSfx
// Sound lumps are DMX format: a format number (3), the sample rate,
// the sample count, then 8-bit unsigned samples.
//
static void LoadSfx (int sfxid)
{
    char	name[16];
    int		lump;
    int		size;
    const byte*	data;
    int		format;
    int		rate;
    int		length;

    snprintf (name, sizeof(name), "ds%s", S_sfx[sfxid].name);
    lump = W_CheckNumForName (name);
    if (lump < 0)
	return;	// eg. DOOM II sounds asked for by DOOM

    size = W_LumpLength (lump);
    if (size < 8)
	return;

    data = W_CacheLumpNum (lump, PU_STATIC);
    format = data[0] | (data[1] << 8);
    rate = data[2] | (data[3] << 8);
    length = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);

    if (format != 3 || rate == 0)
	return;
    if (length > size - 8)
	length = size - 8;

    // DMX skipped 16 bytes of padding at either end.
    data += 8;
    if (length > 32)
    {
	data += 16;
	length -= 32;
    }

    sfxdata[sfxid].samples = data;
    sfxdata[sfxid].length = length;
    sfxdata[sfxid].rate = rate;
}

static void CacheSounds (void)
{
    int		i;
    int		link;

    for (i=1 ; i<NUMSFX ; i++)
    {
	if (S_sfx[i].link)
	    continue;
	LoadSfx (i);
	S_sfx[i].data = (void *)sfxdata[i].samples;
    }

    // Linked sounds share their target's samples.
    for (i=1 ; i<NUMSFX ; i++)
    {
	if (!S_sfx[i].link)
	    continue;
	link = S_sfx[i].link - S_sfx;
	sfxdata[i] = sfxdata[link];
	S_sfx[i].data = S_sfx[link].data;
    }
}


//
// MIXING
//

static void SetChannelParams (mixchannel_t* c, int vol, int sep, int pitch)
{
    float	left;
    float	right;
    double	ratio;

    // DMX panned linearly: the far side fades out as the near side
    // gets up to twice as loud.
    left = ((254 - sep) * vol) / 127.0f;
    right = (sep * vol) / 127.0f;
    if (left < 0) left = 0;
    if (left > 255) left = 255;
    if (right < 0) right = 0;
    if (right > 255) right = 255;
    c->leftgain = left / 255.0f;
    c->rightgain = right / 255.0f;

    ratio = (double)c->sfx->rate / outrate;
    if (snd_pitchshift)
	ratio *= pow (2.0, (pitch - NORM_PITCH) / 64.0);
    c->step = (Uint64)(ratio * 4294967296.0);
}

//
// SampleAt
// 4-point Hermite interpolation between the low rate samples,
// a smooth reconstruction of what a Sound Blaster's filter did.
//
static inline float SampleAt (const sfxdata_t* sfx, Uint64 pos)
{
    int		i;
    float	t;
    float	s0;
    float	s1;
    float	s2;
    float	s3;
    float	c1;
    float	c2;
    float	c3;

    i = (int)(pos >> 32);
    t = (float)(pos & 0xffffffffu) / 4294967296.0f;

    s1 = sfx->samples[i] - 128.0f;
    s0 = i > 0 ? sfx->samples[i-1] - 128.0f : s1;
    s2 = i+1 < sfx->length ? sfx->samples[i+1] - 128.0f : 0;
    s3 = i+2 < sfx->length ? sfx->samples[i+2] - 128.0f : 0;

    c1 = 0.5f * (s2 - s0);
    c2 = s0 - 2.5f*s1 + 2.0f*s2 - 0.5f*s3;
    c3 = 0.5f * (s3 - s0) + 1.5f * (s1 - s2);
    return (((c3*t + c2)*t + c1)*t + s1) / 128.0f;
}

static void MixSfx (float* out, int frames)
{
    int			i;
    int			f;
    mixchannel_t*	c;
    float		s;
    Uint64		end;

    for (i=0 ; i<NUM_MIXCHANNELS ; i++)
    {
	c = &mixchannels[i];
	if (!c->handle)
	    continue;

	end = (Uint64)c->sfx->length << 32;
	for (f=0 ; f<frames ; f++)
	{
	    if (c->pos >= end)
	    {
		c->handle = 0;
		break;
	    }
	    s = SampleAt (c->sfx, c->pos);
	    out[f*2] += s * c->leftgain * SFX_GAIN;
	    out[f*2+1] += s * c->rightgain * SFX_GAIN;
	    c->pos += c->step;
	}
    }
}

static void SDLCALL AudioCallback (void* userdata, SDL_AudioStream* astream,
				   int additional, int total)
{
    int		frames;
    int		chunk;
    int		i;
    float	v;

    frames = additional / (2 * sizeof(float));
    while (frames > 0)
    {
	chunk = frames < mixbufferframes ? frames : mixbufferframes;
	memset (mixbuffer, 0, chunk * 2 * sizeof(float));

	if (!nosfx)
	    MixSfx (mixbuffer, chunk);

	if (musicready)
	{
	    OPLMUS_Render (musicbuffer, chunk);
	    for (i=0 ; i<chunk*2 ; i++)
	    {
		float	in = musicbuffer[i] * (MUSIC_GAIN / 32768.0f);
		float	out = in - dcblockin[i&1] + dcblockcoef * dcblockout[i&1];

		dcblockin[i&1] = in;
		dcblockout[i&1] = out;
		mixbuffer[i] += out;
	    }
	}

	// Soft clip rather than wrap or square off.
	for (i=0 ; i<chunk*2 ; i++)
	{
	    v = mixbuffer[i];
	    if (v > 1.0f || v < -1.0f)
		mixbuffer[i] = tanhf (v);
	}

	SDL_PutAudioStreamData (astream, mixbuffer, chunk * 2 * sizeof(float));
	frames -= chunk;
    }
}


static void LockAudio (void)
{
    if (stream)
	SDL_LockAudioStream (stream);
}

static void UnlockAudio (void)
{
    if (stream)
	SDL_UnlockAudioStream (stream);
}


//
// SFX API
//

void I_SetChannels()
{
    // Mixing channels are set up in I_InitSound.
}

void I_SetSfxVolume(int volume)
{
    // s_sound scales each sound's volume itself.
}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[16];

    snprintf (namebuf, sizeof(namebuf), "ds%s", sfx->name);
    return W_CheckNumForName (namebuf);
}

static mixchannel_t* ChannelForHandle (int handle)
{
    int		i;

    if (handle <= 0)
	return NULL;
    for (i=0 ; i<NUM_MIXCHANNELS ; i++)
	if (mixchannels[i].handle == handle)
	    return &mixchannels[i];
    return NULL;
}

//
// I_StartSound
// Returns a handle for the playing sound, or 0.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{
    int			i;
    mixchannel_t*	c;
    int			handle;

    if (!stream || nosfx || id <= 0 || id >= NUMSFX || !sfxdata[id].samples)
	return 0;

    LockAudio ();

    // A free channel, or else the one that has played longest.
    c = NULL;
    for (i=0 ; i<NUM_MIXCHANNELS ; i++)
    {
	if (!mixchannels[i].handle)
	{
	    c = &mixchannels[i];
	    break;
	}
	if (!c || mixchannels[i].handle < c->handle)
	    c = &mixchannels[i];
    }

    handle = nexthandle++;
    if (nexthandle <= 0)
	nexthandle = 1;

    c->sfx = &sfxdata[id];
    c->sfxid = id;
    c->pos = 0;
    SetChannelParams (c, vol, sep, pitch);
    c->handle = handle;

    UnlockAudio ();
    return handle;
}

void I_StopSound (int handle)
{
    mixchannel_t*	c;

    LockAudio ();
    c = ChannelForHandle (handle);
    if (c)
	c->handle = 0;
    UnlockAudio ();
}

int I_SoundIsPlaying(int handle)
{
    int		playing;

    LockAudio ();
    playing = ChannelForHandle (handle) != NULL;
    UnlockAudio ();
    return playing;
}

void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
    mixchannel_t*	c;

    LockAudio ();
    c = ChannelForHandle (handle);
    if (c)
	SetChannelParams (c, vol, sep, pitch);
    UnlockAudio ();
}


//
// Mixing happens in the audio callback.
//
void I_UpdateSound( void )
{
}

void I_SubmitSound(void)
{
}



void I_ShutdownSound(void)
{
    if (!stream)
	return;

    I_ShutdownMusic ();
    SDL_DestroyAudioStream (stream);
    stream = NULL;
    SDL_QuitSubSystem (SDL_INIT_AUDIO);
}


void
I_InitSound()
{
    SDL_AudioSpec	spec;

    nosfx = M_CheckParm ("-nosfx") || M_CheckParm ("-nosound");
    nomusic = M_CheckParm ("-nomusic") || M_CheckParm ("-nosound");
    if (nosfx && nomusic)
	return;

    // Ask for small device buffers; the default can lag noticeably.
    SDL_SetHint (SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");

    if (!SDL_InitSubSystem (SDL_INIT_AUDIO))
    {
	fprintf (stderr, "I_InitSound: %s\n", SDL_GetError ());
	return;
    }

    outrate = snd_samplerate;
    if (outrate < 11025 || outrate > 192000)
	outrate = 48000;

    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = outrate;

    dcblockcoef = 1.0f - 2.0f * 3.14159265f * 10.0f / outrate;

    mixbufferframes = 1024;
    mixbuffer = malloc (mixbufferframes * 2 * sizeof(float));
    musicbuffer = malloc (mixbufferframes * 2 * sizeof(short));

    stream = SDL_OpenAudioDeviceStream (SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
					&spec, AudioCallback, NULL);
    if (!stream)
    {
	fprintf (stderr, "I_InitSound: could not open audio: %s\n",
		 SDL_GetError ());
	SDL_QuitSubSystem (SDL_INIT_AUDIO);
	return;
    }

    if (!nosfx)
	CacheSounds ();
    I_InitMusic ();

    SDL_ResumeAudioStreamDevice (stream);
    fprintf (stderr, "I_InitSound: %d Hz stereo\n", outrate);
}




//
// MUSIC API.
//

static const byte*	songdata;
static int		songlength;

void I_InitMusic(void)
{
    int		lump;
    const byte*	genmidi;

    if (!stream || nomusic)
	return;

    lump = W_CheckNumForName ("GENMIDI");
    if (lump < 0)
    {
	fprintf (stderr, "I_InitMusic: no GENMIDI lump, music disabled\n");
	return;
    }
    genmidi = W_CacheLumpNum (lump, PU_STATIC);

    LockAudio ();
    musicready = OPLMUS_Init (outrate, opl_type != 2,
			      genmidi, W_LumpLength (lump));
    UnlockAudio ();

    if (!musicready)
	fprintf (stderr, "I_InitMusic: bad GENMIDI lump, music disabled\n");
    else
	fprintf (stderr, "I_InitMusic: %s music\n",
		 opl_type != 2 ? "OPL3" : "OPL2");
}

void I_ShutdownMusic(void)
{
    if (!musicready)
	return;

    LockAudio ();
    OPLMUS_Shutdown ();
    musicready = false;
    UnlockAudio ();
}

void I_SetMusicVolume(int volume)
{
    // Internal state variable.
    snd_MusicVolume = volume;

    LockAudio ();
    OPLMUS_SetVolume (volume);
    UnlockAudio ();
}

void I_PlaySong(int handle, int looping)
{
    if (!musicready || !songdata)
	return;

    LockAudio ();
    OPLMUS_Play (songdata, songlength, looping);
    UnlockAudio ();
}

void I_PauseSong (int handle)
{
    LockAudio ();
    OPLMUS_Pause (true);
    UnlockAudio ();
}

void I_ResumeSong (int handle)
{
    LockAudio ();
    OPLMUS_Pause (false);
    UnlockAudio ();
}

void I_StopSong(int handle)
{
    LockAudio ();
    OPLMUS_Stop ();
    UnlockAudio ();
}

void I_UnRegisterSong(int handle)
{
    LockAudio ();
    OPLMUS_Stop ();
    songdata = NULL;
    songlength = 0;
    UnlockAudio ();
}

int I_RegisterSong(void* data, int length)
{
    if (!OPLMUS_IsMUS (data, length))
    {
	// PWADs often carry MIDI music, which isn't supported yet.
	fprintf (stderr, "I_RegisterSong: not a MUS lump, skipping\n");
	songdata = NULL;
	return 0;
    }

    songdata = data;
    songlength = length;
    return 1;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
    int		playing;

    LockAudio ();
    playing = OPLMUS_Playing ();
    UnlockAudio ();
    return playing;
}
