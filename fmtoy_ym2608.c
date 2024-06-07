#include <math.h>

#include "fmtoy.h"
#include "fmtoy_ym2608.h"
#include "chips/fm.h"

static int fmtoy_ym2608_init(struct fmtoy *fmtoy, int clock, int sample_rate, struct fmtoy_channel *channel) {
	channel->chip->clock = clock;
	channel->chip->data = ym2608_init(0, clock, sample_rate, 0, 0, 0);
	ym2608_reset_chip(channel->chip->data);

	// Enable 6 channel mode
	ym2608_write(channel->chip->data, 0, 0x29);
	ym2608_write(channel->chip->data, 1, 0x80);

	return 0;
}

static int fmtoy_ym2608_destroy(struct fmtoy *fmtoy, struct fmtoy_channel *channel) {
	return 0;
}

static void fmtoy_ym2608_program_change(struct fmtoy *fmtoy, uint8_t program, struct fmtoy_channel *channel) {
	struct opn_voice *v = &fmtoy->opn_voices[program];
	for(int i = 0; i < 6; i++) {
		int base = i < 3 ? 0 : 2;
		int c = i % 3;
		ym2608_write(channel->chip->data, base+0, 0xb0 + c);
		ym2608_write(channel->chip->data, base+1, v->fb_con);
		ym2608_write(channel->chip->data, base+0, 0xb4 + c);
		ym2608_write(channel->chip->data, base+1, v->lr_ams_pms);
		for(int j = 0; j < 4; j++) {
			struct opn_voice_operator *op = &v->operators[j];
			ym2608_write(channel->chip->data, base+0, 0x30 + c + j * 4);
			ym2608_write(channel->chip->data, base+1, op->dt_mul);
			ym2608_write(channel->chip->data, base+0, 0x40 + c + j * 4);
			ym2608_write(channel->chip->data, base+1, op->tl);
			ym2608_write(channel->chip->data, base+0, 0x50 + c + j * 4);
			ym2608_write(channel->chip->data, base+1, op->ks_ar);
			ym2608_write(channel->chip->data, base+0, 0x60 + c + j * 4);
			ym2608_write(channel->chip->data, base+1, op->am_dr);
			ym2608_write(channel->chip->data, base+0, 0x70 + c + j * 4);
			ym2608_write(channel->chip->data, base+1, op->sr);
			ym2608_write(channel->chip->data, base+0, 0x80 + c + j * 4);
			ym2608_write(channel->chip->data, base+1, op->sl_rr);
			ym2608_write(channel->chip->data, base+0, 0x90 + c + j * 4);
			ym2608_write(channel->chip->data, base+1, op->ssg_eg);
		}
	}
}

static void fmtoy_ym2608_set_pitch(struct fmtoy *fmtoy, int chip_channel, float pitch, struct fmtoy_channel *channel) {
	int block_fnum = opnx_pitch_to_block_fnum(pitch, channel->chip->clock);
	int base = chip_channel < 3 ? 0 : 2;
	chip_channel = chip_channel % 3;
	ym2608_write(channel->chip->data, base+0, 0xa4 + chip_channel);
	ym2608_write(channel->chip->data, base+1, block_fnum >> 8);
	ym2608_write(channel->chip->data, base+0, 0xa0 + chip_channel);
	ym2608_write(channel->chip->data, base+1, block_fnum & 0xff);
}

static void fmtoy_ym2608_pitch_bend(struct fmtoy *fmtoy, uint8_t chip_channel, float pitch, struct fmtoy_channel *channel) {
	fmtoy_ym2608_set_pitch(fmtoy, chip_channel, pitch, channel);
}

static void fmtoy_ym2608_note_on(struct fmtoy *fmtoy, uint8_t chip_channel, float pitch, uint8_t velocity, struct fmtoy_channel *channel) {
	fmtoy_ym2608_set_pitch(fmtoy, chip_channel, pitch, channel);
	chip_channel = chip_channel < 3 ? chip_channel : (chip_channel + 1);
	// ym2608_write(channel->chip->data, 0, 0x28);
	// ym2608_write(channel->chip->data, 1, chip_channel);
	ym2608_write(channel->chip->data, 0, 0x28);
	ym2608_write(channel->chip->data, 1, 0xf0 + chip_channel);
}

static void fmtoy_ym2608_note_off(struct fmtoy *fmtoy, uint8_t chip_channel, uint8_t velocity, struct fmtoy_channel *channel) {
	chip_channel = chip_channel < 3 ? chip_channel : chip_channel + 1;
	ym2608_write(channel->chip->data, 0, 0x28);
	ym2608_write(channel->chip->data, 1, chip_channel);
}

static void fmtoy_ym2608_render(struct fmtoy *fmtoy, stream_sample_t **buffers, int num_samples, struct fmtoy_channel *channel) {
	ym2608_update_one(channel->chip->data, buffers, num_samples);
}

struct fmtoy_chip fmtoy_chip_ym2608 = {
	.name = "YM2608",
	.init = fmtoy_ym2608_init,
	.destroy = fmtoy_ym2608_destroy,
	.program_change = fmtoy_ym2608_program_change,
	.pitch_bend = fmtoy_ym2608_pitch_bend,
	.note_on = fmtoy_ym2608_note_on,
	.note_off = fmtoy_ym2608_note_off,
	.render = fmtoy_ym2608_render,
	.max_poliphony = 6,
};
