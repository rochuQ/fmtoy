#include <jack/jack.h>
#include <asoundlib.h>

#include "cmdline.h"
#include "tools.h"
#include "opm_file.h"
#include "fmtoy.h"

typedef jack_default_audio_sample_t sample_t;

#define DEFAULT_CLIENT_NAME "FM Toy"
char *opt_name = DEFAULT_CLIENT_NAME;
char *opt_port = 0;
int opt_verbose = 0;

jack_client_t *client;
jack_port_t *output_ports[2];
unsigned long sr;

struct fmtoy fmtoy;

int process(jack_nframes_t nframes, void *arg) {
	sample_t *buffers[2];
	buffers[0] = (sample_t *) jack_port_get_buffer(output_ports[0], nframes);
	buffers[1] = (sample_t *) jack_port_get_buffer(output_ports[1], nframes);

	fmtoy_render(&fmtoy, nframes);

	for(int i = 0; i < nframes; i++) {
		buffers[0][i] = fmtoy.render_buf_l[i] / 16383.0f;
		buffers[1][i] = fmtoy.render_buf_r[i] / 16383.0f;
	}

	return 0;
}

void midi_action(snd_seq_t *seq_handle) {
	snd_seq_event_t *ev;

	do {
		snd_seq_event_input(seq_handle, &ev);
		switch (ev->type) {
			case SND_SEQ_EVENT_NOTEON:
				if(opt_verbose)
					printf("%d: Note on %s (%d) %d\n", ev->data.note.channel, midi_note_name(ev->data.note.note), ev->data.note.note, ev->data.note.velocity);
				fmtoy_note_on(&fmtoy, ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
				break;
			case SND_SEQ_EVENT_NOTEOFF:
				if(opt_verbose)
					printf("%d: Note off %s (%d) %d\n", ev->data.note.channel, midi_note_name(ev->data.note.note), ev->data.note.note, ev->data.note.velocity);
				fmtoy_note_off(&fmtoy, ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
				break;
			case SND_SEQ_EVENT_PITCHBEND:
				fmtoy_pitch_bend(&fmtoy, ev->data.control.channel, ev->data.control.value);
				break;
			case SND_SEQ_EVENT_PGMCHANGE:
				if(opt_verbose)
					printf("%d: Program %d\n", ev->data.control.channel, ev->data.control.value);
				fmtoy_program_change(&fmtoy, ev->data.control.channel, ev->data.control.value);
				break;
			case SND_SEQ_EVENT_CONTROLLER:
				if(opt_verbose)
					printf("%d: CC 0x%02x (%s) %d\n", ev->data.control.channel, ev->data.control.param, midi_cc_name(ev->data.control.param), ev->data.control.value);
				fmtoy_cc(&fmtoy, ev->data.control.channel, ev->data.control.param, ev->data.control.value);
				break;
		}
		snd_seq_free_event(ev);
	} while (snd_seq_event_input_pending(seq_handle, 0) > 0);
}

static void init_jack(void) {
	/* Initial Jack setup, get sample rate */
	jack_status_t status;
	if ((client = jack_client_open (opt_name, JackNoStartServer, &status)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		exit(1);
	}
	jack_set_process_callback(client, process, 0);
	output_ports[0] = jack_port_register(client, "Left",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	output_ports[1] = jack_port_register(client, "Right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	sr = jack_get_sample_rate(client);
}

static void activate_jack(void) {
	if (jack_activate(client)) {
		fprintf (stderr, "cannot activate client");
		exit (1);
	}

	const char **ports;
	ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL) {
		fprintf(stderr, "no physical capture ports\n");
		exit (1);
	}

	if (jack_connect (client, jack_port_name (output_ports[0]), ports[0])) {
		fprintf (stderr, "cannot connect input ports\n");
	}

	if (jack_connect (client, jack_port_name (output_ports[1]), ports[1])) {
		fprintf (stderr, "cannot connect input ports\n");
	}
}

static snd_seq_t *seq_handle;
void init_seq() {
	int portid;

	if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
		fprintf(stderr, "Error opening ALSA sequencer.\n");
		exit(1);
	}
	snd_seq_set_client_name(seq_handle, opt_name);
	if ((portid = snd_seq_create_simple_port(seq_handle, "MIDI In",
						SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
						SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
		fprintf(stderr, "Error creating sequencer port.\n");
		exit(1);
	}

	snd_seq_addr_t seq_input_port;
	if(opt_port) {
		if(snd_seq_parse_address(seq_handle, &seq_input_port, opt_port) == 0) {
			snd_seq_connect_from(seq_handle, portid, seq_input_port.client, seq_input_port.port);
		} else {
			fprintf(stderr, "Could not parse port: %s\n", opt_port);
			exit(1);
		}
	}
}

void do_polling(void) {
	int npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
	struct pollfd *pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
	snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);
	while (1) {
		if (poll(pfd, npfd, 100000) > 0) {
			midi_action(seq_handle);
		}
	}
}

int main(int argc, char **argv) {
	int optind = cmdline_parse_args(argc, argv, (struct cmdline_option[]){
		{
			'n', "name",
			"JACK client name",
			"name",
			TYPE_OPTIONAL,
			TYPE_STRING, &opt_name
		},
		{
			'p', "port",
			"Connect to ALSA seq port for MIDI input",
			"X:Y",
			TYPE_OPTIONAL,
			TYPE_STRING, &opt_port
		},
		{
			'v', "verbose",
			"Output MIDI event names",
			0,
			TYPE_SWITCH,
			TYPE_INT, &opt_verbose
		},
		CMDLINE_ARG_TERMINATOR
	}, 1, 0, "voice.opm/dmp/ins/tfi/y12");

	if(optind < 0) exit(-optind);

	init_jack();

	fmtoy_init(&fmtoy, sr);
	for(int i = optind; i < argc; i++)
		fmtoy_load_voice(&fmtoy, argv[i]);
	for(int i = 0; i < 16; i++)
		fmtoy_program_change(&fmtoy, i, 0);

	activate_jack();
	init_seq();

	do_polling();
}
