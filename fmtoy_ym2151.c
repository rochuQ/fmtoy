#include <math.h>
#include "fmtoy.h"
#include "fmtoy_ym2151.h"
#include "tools.h"

static int fmtoy_ym2151_init(struct fmtoy *fmtoy, int clock, int sample_rate, struct fmtoy_channel *channel) {
	channel->chip->clock = clock;
	channel->chip->data = ym2151_init(clock, sample_rate);
	ym2151_reset_chip(channel->chip->data);
	return 0;
}

static int fmtoy_ym2151_destroy(struct fmtoy *fmtoy, struct fmtoy_channel *channel) {
	return 0;
}

static void fmtoy_ym2151_program_change(struct fmtoy *fmtoy, uint8_t program, struct fmtoy_channel *channel) {
	struct fmtoy_opm_voice *v = &fmtoy->opm_voices[program];

	// prepare for velocity
	channel->con = v->rl_fb_con & 0x07;
	channel->op_mask = v->sm;
	for(int j = 0; j < 4; j++)
		channel->tl[j] = v->operators[j].tl;

	for(int i = 0; i < 8; i++) {
		ym2151_write_reg(channel->chip->data, 0x20 + i, v->rl_fb_con);
		ym2151_write_reg(channel->chip->data, 0x38 + i, v->pms_ams);
		for(int j = 0; j < 4; j++) {
			struct fmtoy_opm_voice_operator *op = &v->operators[j];
			ym2151_write_reg(channel->chip->data, 0x40 + i + j * 8, op->dt1_mul);
			ym2151_write_reg(channel->chip->data, 0x60 + i + j * 8, op->tl);
			ym2151_write_reg(channel->chip->data, 0x80 + i + j * 8, op->ks_ar);
			ym2151_write_reg(channel->chip->data, 0xa0 + i + j * 8, op->ame_d1r);
			ym2151_write_reg(channel->chip->data, 0xc0 + i + j * 8, op->dt2_d2r);
			ym2151_write_reg(channel->chip->data, 0xe0 + i + j * 8, op->d1l_rr);
		}
	}
}

static void fmtoy_ym2151_set_pitch(struct fmtoy *fmtoy, uint8_t chip_channel, float pitch, struct fmtoy_channel *channel) {
	float kf = 3584 + 64 * 12 * log2(pitch * 3579545.0 / channel->chip->clock / 440.0);
	int octave = (int)kf / 64 / 12;
	int k = ((int)kf / 64) % 12;
	const uint8_t opm_notes[12] = { 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14 };
	ym2151_write_reg(channel->chip->data, 0x28 + chip_channel, octave << 4 | opm_notes[k]);
	ym2151_write_reg(channel->chip->data, 0x30 + chip_channel, (int)kf % 64 << 2);
}

static void fmtoy_ym2151_note_on(struct fmtoy *fmtoy, uint8_t chip_channel, float pitch, uint8_t velocity, struct fmtoy_channel *channel) {
	fmtoy_ym2151_set_pitch(fmtoy, chip_channel, pitch, channel);

	// set velocity
	// C2
	if(channel->op_mask & 0x40)
		ym2151_write_reg(channel->chip->data, 0x78 + chip_channel, MIN(127, channel->tl[3] + tl));
	// C1
	if(channel->op_mask & 0x10 && channel->con >= 4)
		ym2151_write_reg(channel->chip->data, 0x70 + chip_channel, MIN(127, channel->tl[2] + tl));
	// M2
	if(channel->op_mask & 0x20 && channel->con >= 5)
		ym2151_write_reg(channel->chip->data, 0x68 + chip_channel, MIN(127, channel->tl[1] + tl));
	// M1
	if(channel->op_mask & 0x08 && channel->con >= 7)
		ym2151_write_reg(channel->chip->data, 0x60 + chip_channel, MIN(127, channel->tl[0] + tl));

	// key on
	ym2151_write_reg(channel->chip->data, 0x08, channel->op_mask | (chip_channel & 0x07));
}

static void fmtoy_ym2151_note_off(struct fmtoy *fmtoy, uint8_t chip_channel, uint8_t velocity, struct fmtoy_channel *channel) {
	ym2151_write_reg(channel->chip->data, 0x08, chip_channel & 0x07);
}

static void fmtoy_ym2151_pitch_bend(struct fmtoy *fmtoy, uint8_t chip_channel, float pitch, struct fmtoy_channel *channel) {
	fmtoy_ym2151_set_pitch(fmtoy, chip_channel, pitch, channel);
}

static void fmtoy_ym2151_render(struct fmtoy *fmtoy, stream_sample_t **buffers, int num_samples, struct fmtoy_channel *channel) {
	ym2151_update_one(channel->chip->data, buffers, num_samples);
}

struct fmtoy_chip fmtoy_chip_ym2151 = {
	.name = "YM2151",
	.init = fmtoy_ym2151_init,
	.destroy = fmtoy_ym2151_destroy,
	.program_change = fmtoy_ym2151_program_change,
	.pitch_bend = fmtoy_ym2151_pitch_bend,
	.note_on = fmtoy_ym2151_note_on,
	.note_off = fmtoy_ym2151_note_off,
	.render = fmtoy_ym2151_render,
	.max_poliphony = 8,
};
