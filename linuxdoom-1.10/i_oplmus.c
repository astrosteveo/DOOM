//-----------------------------------------------------------------------------
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
// DESCRIPTION:
//	MUS music on an emulated OPL3, played the way the DMX sound
//	library did on a Sound Blaster: GENMIDI instruments from the
//	WAD, DMX's voice allocation, volume curve and frequency table.
//
//	Everything here runs on the audio thread; the caller holds the
//	audio lock around every entry point.
//
//-----------------------------------------------------------------------------

#include <string.h>
#include <math.h>

#include "doomtype.h"
#include "m_swap.h"

#include "opl3.h"
#include "i_oplmus.h"


//
// GENMIDI lump layout.
//
#define GENMIDI_HEADER		"#OPL_II#"
#define GENMIDI_NUMINSTRS	128
#define GENMIDI_NUMPERCUSSION	47
#define GENMIDI_FIRSTPERCUSSION	35

#define GENMIDI_FLAG_FIXED	0x0001	// fixed pitch
#define GENMIDI_FLAG_2VOICE	0x0004	// double voice

typedef struct
{
    byte	tremolo;	// AM, vibrato, sustain, KSR, multiplier
    byte	attack;
    byte	sustain;
    byte	waveform;
    byte	scale;		// key scale level, top two bits
    byte	level;
} __attribute__((packed)) genmidi_op_t;

typedef struct
{
    genmidi_op_t	modulator;
    byte		feedback;
    genmidi_op_t	carrier;
    byte		unused;
    short		base_note_offset;
} __attribute__((packed)) genmidi_voice_t;

typedef struct
{
    unsigned short	flags;
    byte		fine_tuning;
    byte		fixed_note;
    genmidi_voice_t	voices[2];
} __attribute__((packed)) genmidi_instr_t;


//
// OPL registers.
//
#define OPL_REG_WAVEFORM_ENABLE	0x01
#define OPL_REG_TIMER_CTRL	0x04
#define OPL_REG_FM_MODE		0x08
#define OPL_REG_NEW		0x105

#define OPL_REGS_TREMOLO	0x20
#define OPL_REGS_LEVEL		0x40
#define OPL_REGS_ATTACK		0x60
#define OPL_REGS_SUSTAIN	0x80
#define OPL_REGS_WAVEFORM	0xe0
#define OPL_REGS_FREQ_1		0xa0
#define OPL_REGS_FREQ_2		0xb0
#define OPL_REGS_FEEDBACK	0xc0

#define OPL_NUM_VOICES		9

// Modulator operator of each of the nine channels in a register bank;
// the carrier is always three above.
static const int voice_operators[OPL_NUM_VOICES] =
{
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12
};

// DMX's mapping of MIDI volume onto OPL attenuation.
static const byte volume_mapping_table[128] =
{
    0, 1, 3, 5, 6, 8, 10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127
};

// DMX looks frequencies up in steps of 1/32 semitone. The first 284
// entries cover the lowest notes; above that one octave of F-numbers
// repeats, with the block (octave) number added.
#define FREQ_TABLE_LOW		284
#define FREQ_TABLE_OCTAVE	(12*32)

static unsigned short	frequency_curve[FREQ_TABLE_LOW + FREQ_TABLE_OCTAVE];


//
// MUS format.
//
typedef struct
{
    char		id[4];		// "MUS" 0x1a
    unsigned short	scorelen;
    unsigned short	scorestart;
    unsigned short	primarychannels;
    unsigned short	secondarychannels;
    unsigned short	instrcnt;
    unsigned short	dummy;
} __attribute__((packed)) musheader_t;

#define MUS_NUMCHANNELS		16
#define MUS_PERCUSSION		15
#define MUS_TICRATE		140

enum
{
    mus_releasekey,
    mus_presskey,
    mus_pitchwheel,
    mus_systemevent,
    mus_changecontroller,
    mus_measureend,
    mus_scoreend,
    mus_unused
};

enum
{
    musctrl_instrument,
    musctrl_bank,
    musctrl_modulation,
    musctrl_volume,
    musctrl_pan,
    musctrl_expression,
    musctrl_reverb,
    musctrl_chorus,
    musctrl_sustain,
    musctrl_soft
};

enum
{
    mussys_allsoundsoff = 10,
    mussys_allnotesoff,
    mussys_mono,
    mussys_poly,
    mussys_resetallcontrollers
};


typedef struct
{
    const genmidi_instr_t*	instrument;
    int		volume;		// as limited by the music volume
    int		volume_base;	// as set by the song
    int		pan;		// stereo bits for the feedback register
    int		bend;		// -64..63, 32 steps a semitone
    int		lastvolume;	// MUS notes may reuse the previous volume
} channel_t;

typedef struct
{
    int		index;		// 0..8 within its register bank
    int		array;		// 0 or 0x100 for the second bank
    int		op1;		// modulator operator
    int		op2;		// carrier operator

    channel_t*	channel;	// NULL when free
    const genmidi_instr_t*	instr;
    int		instrvoice;	// 0 or 1 for double voice instruments
    int		key;		// key that started it
    int		note;		// note actually played
    int		note_volume;
    int		freq;		// last value written to the freq registers
    int		car_volume;	// last values written to the level registers
    int		mod_volume;
    int		reg_pan;
} voice_t;


static opl3_chip	chip;
static int		samplerate;
static boolean		opl3mode;
static boolean		initialized;

static const genmidi_instr_t*	main_instrs;
static const genmidi_instr_t*	percussion_instrs;

static channel_t	channels[MUS_NUMCHANNELS];
static voice_t		voices[OPL_NUM_VOICES*2];
static int		numvoices;

// Voices in the order DMX hands them out: free voices are taken from
// the front and returned to the back, allocated ones are kept in the
// order they started so the oldest can be found.
static voice_t*		voice_free_list[OPL_NUM_VOICES*2];
static int		voice_free_num;
static voice_t*		voice_alloced_list[OPL_NUM_VOICES*2];
static int		voice_alloced_num;

static int		music_volume = 127;

// The song.
static const byte*	score;
static int		scorelen;
static int		scorepos;
static boolean		looping;
static boolean		playing;
static boolean		paused;
static int		delaytics;	// MUS tics to the next event
static int		tickaccum;	// output frames toward the next tic


static void WriteRegister (int reg, int value)
{
    OPL3_WriteReg (&chip, reg, value);
}


static void InitFrequencyCurve (void)
{
    int		i;
    double	note;
    double	freq;

    // Index 0 is two semitones below MUS note 0, and DMX tuned
    // everything an octave above the MIDI standard (GENMIDI's base
    // note offsets bring instruments back down). In block 0 the
    // OPL plays F-number * 49716 / 2^20 Hz.
    for (i=0 ; i<FREQ_TABLE_LOW + FREQ_TABLE_OCTAVE ; i++)
    {
	note = (i - 64) / 32.0 + 12;
	freq = 440.0 * pow (2.0, (note - 69) / 12.0);
	frequency_curve[i] = (unsigned short)(freq * 1048576.0 / 49716.0 + 0.5);
    }
}


//
// Voices
//

static void InitVoices (void)
{
    int		i;

    numvoices = opl3mode ? OPL_NUM_VOICES*2 : OPL_NUM_VOICES;
    voice_free_num = 0;
    voice_alloced_num = 0;

    for (i=0 ; i<numvoices ; i++)
    {
	memset (&voices[i], 0, sizeof(voices[i]));
	voices[i].index = i % OPL_NUM_VOICES;
	voices[i].array = (i / OPL_NUM_VOICES) << 8;
	voices[i].op1 = voice_operators[i % OPL_NUM_VOICES];
	voices[i].op2 = voices[i].op1 + 3;
	voices[i].reg_pan = 0x30;
	voice_free_list[voice_free_num++] = &voices[i];
    }
}

static voice_t* GetFreeVoice (void)
{
    voice_t*	voice;

    if (!voice_free_num)
	return NULL;

    voice = voice_free_list[0];
    voice_free_num--;
    memmove (&voice_free_list[0], &voice_free_list[1],
	     voice_free_num * sizeof(voice_t*));
    voice_alloced_list[voice_alloced_num++] = voice;
    return voice;
}

static void ReleaseVoice (int index)
{
    voice_t*	voice;

    voice = voice_alloced_list[index];
    voice->channel = NULL;
    voice->note = 0;

    voice_alloced_num--;
    memmove (&voice_alloced_list[index], &voice_alloced_list[index+1],
	     (voice_alloced_num - index) * sizeof(voice_t*));
    voice_free_list[voice_free_num++] = voice;
}

static void LoadOperatorData (int operator, const genmidi_op_t* data,
			      boolean max_level, int* volume)
{
    int		level;

    // The level register holds the key scale bits too.
    level = data->scale;
    if (max_level)
	level |= 0x3f;
    else
	level |= data->level;
    *volume = level;

    WriteRegister (OPL_REGS_LEVEL + operator, level);
    WriteRegister (OPL_REGS_TREMOLO + operator, data->tremolo);
    WriteRegister (OPL_REGS_ATTACK + operator, data->attack);
    WriteRegister (OPL_REGS_SUSTAIN + operator, data->sustain);
    WriteRegister (OPL_REGS_WAVEFORM + operator, data->waveform);
}

static void SetVoiceInstrument (voice_t* voice, const genmidi_instr_t* instr,
				int instrvoice)
{
    const genmidi_voice_t*	data;
    boolean			modulating;

    voice->instr = instr;
    voice->instrvoice = instrvoice;
    data = &instr->voices[instrvoice];

    // In additive (non-modulating) mode both operators are heard, so
    // both start silent until SetVoiceVolume; otherwise just the
    // carrier does. DMX loads the carrier first.
    modulating = (data->feedback & 0x01) == 0;
    LoadOperatorData (voice->op2 | voice->array, &data->carrier,
		      true, &voice->car_volume);
    LoadOperatorData (voice->op1 | voice->array, &data->modulator,
		      !modulating, &voice->mod_volume);

    WriteRegister ((OPL_REGS_FEEDBACK + voice->index) | voice->array,
		   data->feedback | voice->reg_pan);
}

static void SetVoiceVolume (voice_t* voice, int volume)
{
    const genmidi_voice_t*	data;
    int		midi_volume;
    int		full_volume;
    int		car_volume;
    int		mod_volume;

    voice->note_volume = volume;
    data = &voice->instr->voices[voice->instrvoice];

    // Note volume times channel volume, both through DMX's curve.
    midi_volume = 2 * (volume_mapping_table[voice->channel->volume] + 1);
    full_volume = (volume_mapping_table[voice->note_volume] * midi_volume) >> 9;
    car_volume = 0x3f - full_volume;

    if (car_volume == (voice->car_volume & 0x3f))
	return;

    voice->car_volume = car_volume | (voice->car_volume & 0xc0);
    WriteRegister ((OPL_REGS_LEVEL + voice->op2) | voice->array,
		   voice->car_volume);

    // In additive mode the modulator is heard too; it may not be
    // louder than the carrier.
    if ((data->feedback & 0x01) && data->modulator.level != 0x3f)
    {
	mod_volume = data->modulator.level;
	if (mod_volume < car_volume)
	    mod_volume = car_volume;
	mod_volume |= voice->mod_volume & 0xc0;

	if (mod_volume != voice->mod_volume)
	{
	    voice->mod_volume = mod_volume;
	    WriteRegister ((OPL_REGS_LEVEL + voice->op1) | voice->array,
			   mod_volume);
	}
    }
}

static int FrequencyForVoice (voice_t* voice)
{
    const genmidi_voice_t*	data;
    int		freq_index;
    int		octave;
    int		sub_index;
    int		note;

    note = voice->note;
    data = &voice->instr->voices[voice->instrvoice];

    if (!(SHORT (voice->instr->flags) & GENMIDI_FLAG_FIXED))
	note += (short)SHORT (data->base_note_offset);

    // The base note offset can push notes out of range.
    while (note < 0)
	note += 12;
    while (note > 95)
	note -= 12;

    freq_index = 64 + 32*note + voice->channel->bend;

    // The second voice of a double voice instrument can be detuned.
    if (voice->instrvoice != 0)
	freq_index += (voice->instr->fine_tuning / 2) - 64;

    if (freq_index < 0)
	freq_index = 0;

    if (freq_index < FREQ_TABLE_LOW)
	return frequency_curve[freq_index];

    sub_index = (freq_index - FREQ_TABLE_LOW) % FREQ_TABLE_OCTAVE;
    octave = (freq_index - FREQ_TABLE_LOW) / FREQ_TABLE_OCTAVE;

    // The register only has three bits of octave.
    if (octave >= 7)
	octave = 7;

    return frequency_curve[sub_index + FREQ_TABLE_LOW] | (octave << 10);
}

static void UpdateVoiceFrequency (voice_t* voice)
{
    int		freq;

    freq = FrequencyForVoice (voice);
    if (voice->freq == freq)
	return;

    // The key-on bit goes with the high frequency bits.
    WriteRegister ((OPL_REGS_FREQ_1 + voice->index) | voice->array,
		   freq & 0xff);
    WriteRegister ((OPL_REGS_FREQ_2 + voice->index) | voice->array,
		   (freq >> 8) | 0x20);
    voice->freq = freq;
}

static void VoiceKeyOff (voice_t* voice)
{
    WriteRegister ((OPL_REGS_FREQ_2 + voice->index) | voice->array,
		   voice->freq >> 8);
}

//
// ReplaceExistingVoice
// With every voice in use, a new note takes one over. Second voices
// of double voice instruments go first, then voices of the highest
// numbered channel; MUS channel order is priority.
//
static void ReplaceExistingVoice (void)
{
    int		i;
    int		result;

    result = 0;
    for (i=0 ; i<voice_alloced_num ; i++)
    {
	if (voice_alloced_list[i]->instrvoice != 0
	    || voice_alloced_list[i]->channel
	       >= voice_alloced_list[result]->channel)
	{
	    result = i;
	}
    }

    VoiceKeyOff (voice_alloced_list[result]);
    ReleaseVoice (result);
}

static void VoiceKeyOn (channel_t* channel, const genmidi_instr_t* instr,
			int instrvoice, int note, int key, int volume)
{
    voice_t*	voice;

    voice = GetFreeVoice ();
    if (!voice)
	return;

    voice->channel = channel;
    voice->key = key;

    // Fixed pitch instruments (mostly percussion) ignore the key.
    if (SHORT (instr->flags) & GENMIDI_FLAG_FIXED)
	voice->note = instr->fixed_note;
    else
	voice->note = note;

    voice->reg_pan = channel->pan;

    SetVoiceInstrument (voice, instr, instrvoice);
    SetVoiceVolume (voice, volume);

    voice->freq = 0;
    UpdateVoiceFrequency (voice);
}


//
// Channel events
//

static void KeyOff (channel_t* channel, int key)
{
    int		i;

    // Releasing shifts the list down, so walk it backwards.
    for (i=voice_alloced_num-1 ; i>=0 ; i--)
    {
	if (voice_alloced_list[i]->channel == channel
	    && voice_alloced_list[i]->key == key)
	{
	    VoiceKeyOff (voice_alloced_list[i]);
	    ReleaseVoice (i);
	}
    }
}

static void KeyOn (int mus_channel, int key, int volume)
{
    channel_t*			channel;
    const genmidi_instr_t*	instr;
    int				note;

    channel = &channels[mus_channel];

    // A zero volume note is a release, as some songs use it.
    if (volume <= 0)
    {
	KeyOff (channel, key);
	return;
    }

    if (mus_channel == MUS_PERCUSSION)
    {
	if (key < GENMIDI_FIRSTPERCUSSION
	    || key >= GENMIDI_FIRSTPERCUSSION + GENMIDI_NUMPERCUSSION)
	    return;
	instr = &percussion_instrs[key - GENMIDI_FIRSTPERCUSSION];
	note = 60;
    }
    else
    {
	instr = channel->instrument;
	note = key;
    }

    if (!voice_free_num)
	ReplaceExistingVoice ();

    VoiceKeyOn (channel, instr, 0, note, key, volume);
    if (SHORT (instr->flags) & GENMIDI_FLAG_2VOICE)
	VoiceKeyOn (channel, instr, 1, note, key, volume);
}

static void AllNotesOff (channel_t* channel)
{
    int		i;

    for (i=voice_alloced_num-1 ; i>=0 ; i--)
    {
	if (voice_alloced_list[i]->channel == channel)
	{
	    VoiceKeyOff (voice_alloced_list[i]);
	    ReleaseVoice (i);
	}
    }
}

static void SetChannelVolume (channel_t* channel, int volume)
{
    int		i;

    // DMX limits channel volumes to the music volume, rather than
    // scaling them by it.
    channel->volume_base = volume;
    if (volume > music_volume)
	volume = music_volume;
    channel->volume = volume;

    for (i=0 ; i<voice_alloced_num ; i++)
	if (voice_alloced_list[i]->channel == channel)
	    SetVoiceVolume (voice_alloced_list[i],
			    voice_alloced_list[i]->note_volume);
}

static void SetChannelPan (channel_t* channel, int pan)
{
    int		reg_pan;
    int		i;
    voice_t*	voice;

    if (!opl3mode)
	return;

    // OPL3 can only send a voice left (0x10), right (0x20) or to both.
    if (pan >= 96)
	reg_pan = 0x20;
    else if (pan <= 48)
	reg_pan = 0x10;
    else
	reg_pan = 0x30;

    if (channel->pan == reg_pan)
	return;
    channel->pan = reg_pan;

    for (i=0 ; i<voice_alloced_num ; i++)
    {
	voice = voice_alloced_list[i];
	if (voice->channel != channel)
	    continue;
	voice->reg_pan = reg_pan;
	WriteRegister ((OPL_REGS_FEEDBACK + voice->index) | voice->array,
		       voice->instr->voices[voice->instrvoice].feedback
		       | reg_pan);
    }
}

static void SetChannelBend (channel_t* channel, int bend)
{
    int		i;

    // MUS bends are 0..255, centered on 128: the top byte of the
    // equivalent MIDI bend is half that.
    channel->bend = (bend >> 1) - 64;

    for (i=0 ; i<voice_alloced_num ; i++)
	if (voice_alloced_list[i]->channel == channel)
	    UpdateVoiceFrequency (voice_alloced_list[i]);
}

static void ResetChannels (void)
{
    int		i;

    for (i=0 ; i<MUS_NUMCHANNELS ; i++)
    {
	channels[i].instrument = &main_instrs[0];
	channels[i].volume_base = 100;
	channels[i].volume = 100 < music_volume ? 100 : music_volume;
	channels[i].pan = 0x30;
	channels[i].bend = 0;
	channels[i].lastvolume = 127;
    }
}


//
// Song playback
//

static int ReadScoreByte (void)
{
    if (scorepos >= scorelen)
    {
	playing = false;
	return 0;
    }
    return score[scorepos++];
}

static void StopAllVoices (void)
{
    int		i;

    for (i=voice_alloced_num-1 ; i>=0 ; i--)
    {
	VoiceKeyOff (voice_alloced_list[i]);
	ReleaseVoice (i);
    }
}

static void Restart (void)
{
    const musheader_t*	header;

    header = (const musheader_t *)score;
    scorepos = SHORT (header->scorestart);
    delaytics = 0;
}

//
// ProcessEvents
// Plays events until one is followed by a delay.
//
static void ProcessEvents (void)
{
    int		descriptor;
    int		type;
    int		mus_channel;
    int		key;
    int		value;
    int		controller;
    channel_t*	channel;

    while (playing && delaytics == 0)
    {
	descriptor = ReadScoreByte ();
	if (!playing)
	    return;

	type = (descriptor >> 4) & 7;
	mus_channel = descriptor & 15;
	channel = &channels[mus_channel];

	switch (type)
	{
	  case mus_releasekey:
	    key = ReadScoreByte () & 0x7f;
	    KeyOff (channel, key);
	    break;

	  case mus_presskey:
	    key = ReadScoreByte ();
	    if (key & 0x80)
		channel->lastvolume = ReadScoreByte () & 0x7f;
	    KeyOn (mus_channel, key & 0x7f, channel->lastvolume);
	    break;

	  case mus_pitchwheel:
	    SetChannelBend (channel, ReadScoreByte ());
	    break;

	  case mus_systemevent:
	    controller = ReadScoreByte () & 0x7f;
	    if (controller == mussys_allsoundsoff
		|| controller == mussys_allnotesoff)
		AllNotesOff (channel);
	    else if (controller == mussys_resetallcontrollers)
		SetChannelBend (channel, 128);
	    break;

	  case mus_changecontroller:
	    controller = ReadScoreByte () & 0x7f;
	    value = ReadScoreByte () & 0x7f;
	    switch (controller)
	    {
	      case musctrl_instrument:
		channel->instrument = &main_instrs[value];
		break;
	      case musctrl_volume:
		SetChannelVolume (channel, value);
		break;
	      case musctrl_pan:
		SetChannelPan (channel, value);
		break;
	      default:
		// DMX's OPL driver ignores the rest.
		break;
	    }
	    break;

	  case mus_measureend:
	    break;

	  case mus_scoreend:
	    if (looping)
	    {
		Restart ();
		continue;
	    }
	    StopAllVoices ();
	    playing = false;
	    return;

	  default:
	    // Unknown event; give up on the song rather than misread it.
	    StopAllVoices ();
	    playing = false;
	    return;
	}

	// The last event of a group carries the delay after it.
	if (descriptor & 0x80)
	{
	    do
	    {
		value = ReadScoreByte ();
		delaytics = delaytics*128 + (value & 0x7f);
	    } while ((value & 0x80) && playing);
	}
    }
}


//
// API
//

boolean OPLMUS_Init (int rate, boolean opl3, const byte* genmidi, int genmidilen)
{
    int		r;

    if (!genmidi
	|| genmidilen < 8 + (GENMIDI_NUMINSTRS + GENMIDI_NUMPERCUSSION)
			    * (int)sizeof(genmidi_instr_t)
	|| memcmp (genmidi, GENMIDI_HEADER, 8))
	return false;

    main_instrs = (const genmidi_instr_t *)(genmidi + 8);
    percussion_instrs = main_instrs + GENMIDI_NUMINSTRS;

    samplerate = rate;
    opl3mode = opl3;
    InitFrequencyCurve ();

    OPL3_Reset (&chip, samplerate);

    // Silence every operator, as DMX did at startup.
    for (r=0 ; r<0x16 ; r++)
    {
	WriteRegister (OPL_REGS_LEVEL + r, 0x3f);
	WriteRegister (0x100 | (OPL_REGS_LEVEL + r), 0x3f);
    }
    WriteRegister (OPL_REG_TIMER_CTRL, 0x60);
    WriteRegister (OPL_REG_TIMER_CTRL, 0x80);
    WriteRegister (OPL_REG_WAVEFORM_ENABLE, 0x20);
    if (opl3mode)
	WriteRegister (OPL_REG_NEW, 0x01);
    WriteRegister (OPL_REG_FM_MODE, 0x40);

    InitVoices ();
    ResetChannels ();
    initialized = true;
    return true;
}

void OPLMUS_Shutdown (void)
{
    if (initialized)
	StopAllVoices ();
    initialized = false;
    playing = false;
    score = NULL;
}

void OPLMUS_SetVolume (int volume)
{
    int		i;

    music_volume = volume;
    if (!initialized)
	return;
    for (i=0 ; i<MUS_NUMCHANNELS ; i++)
	SetChannelVolume (&channels[i], channels[i].volume_base);
}

boolean OPLMUS_IsMUS (const byte* data, int len)
{
    return len >= (int)sizeof(musheader_t) && !memcmp (data, "MUS\x1a", 4);
}

void OPLMUS_Play (const byte* data, int len, boolean loop)
{
    const musheader_t*	header;
    int			start;

    OPLMUS_Stop ();
    if (!initialized || !OPLMUS_IsMUS (data, len))
	return;

    header = (const musheader_t *)data;
    start = SHORT (header->scorestart);
    if (start >= len)
	return;

    score = data;
    scorelen = start + SHORT (header->scorelen);
    if (scorelen > len)
	scorelen = len;
    looping = loop;
    paused = false;
    tickaccum = 0;

    ResetChannels ();
    Restart ();
    playing = true;
}

void OPLMUS_Stop (void)
{
    if (initialized)
	StopAllVoices ();
    playing = false;
    paused = false;
    score = NULL;
}

void OPLMUS_Pause (boolean pause)
{
    int		i;

    if (pause == paused)
	return;
    paused = pause;

    // DMX lets percussion ring out but cuts the instruments.
    if (paused)
    {
	for (i=voice_alloced_num-1 ; i>=0 ; i--)
	{
	    if (voice_alloced_list[i]->channel != &channels[MUS_PERCUSSION])
	    {
		VoiceKeyOff (voice_alloced_list[i]);
		ReleaseVoice (i);
	    }
	}
    }
}

boolean OPLMUS_Playing (void)
{
    return playing;
}

void OPLMUS_Render (short* out, int frames)
{
    int		i;

    if (!initialized)
    {
	memset (out, 0, frames * 2 * sizeof(short));
	return;
    }

    for (i=0 ; i<frames ; i++)
    {
	if (playing && !paused)
	{
	    // Run the score at 140Hz against the output clock.
	    tickaccum += MUS_TICRATE;
	    while (tickaccum >= samplerate)
	    {
		tickaccum -= samplerate;
		if (delaytics > 0)
		    delaytics--;
		ProcessEvents ();
	    }
	}

	OPL3_GenerateResampled (&chip, out);
	out += 2;
    }
}
