#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static snd_seq_t *seq_handle;
static int in_port;

int midi_open(char *name) {
  int ret = snd_seq_open(&seq_handle, name, SND_SEQ_OPEN_INPUT, 0);
  if (ret != 0) {
    return -1;
  }

  ret = snd_seq_set_client_name(seq_handle, "Midi Listener");
  if (ret != 0) {
    return -2;
  }
  in_port = snd_seq_create_simple_port(seq_handle, "listen:in",
                                       SND_SEQ_PORT_CAP_WRITE |
                                           SND_SEQ_PORT_CAP_SUBS_WRITE,
                                       SND_SEQ_PORT_TYPE_APPLICATION);
  if (in_port < 0) {
    return in_port - 3;
  }
  return 0;
}

int capture_keyboard(int client, int port) {
  int ret = snd_seq_connect_from(seq_handle, in_port, client, port);
  return ret;
}

snd_seq_event_t *midi_read() {
  snd_seq_event_t *ev = NULL;
  int ret = snd_seq_event_input(seq_handle, &ev);
  if (ret < 0) {
      ev = NULL;
  }
  return ev;
}

void midi_process(snd_seq_event_t *ev) {
  if ((ev->type == SND_SEQ_EVENT_NOTEON) ||
      (ev->type == SND_SEQ_EVENT_NOTEOFF)) {
    const char *type = (ev->type == SND_SEQ_EVENT_NOTEON) ? "on " : "off";

    printf("[%d] Note %s: %2x vel(%2x)\n", ev->time.tick, type,
           ev->data.note.note, ev->data.note.velocity);
  } else if (ev->type == SND_SEQ_EVENT_CONTROLLER)
    printf("[%d] Control:  %2x val(%2x)\n", ev->time.tick,
           ev->data.control.param, ev->data.control.value);
  else
    printf("[%d] Unknown:  Unhandled Event Received\n", ev->time.tick);
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: midi-test <sequencer name> <client> <port>(eg. "
                    "midi-test hw 20 0)\n");
    exit(1);
  }

  int sender_client = atoi(argv[2]);
  int sender_port = atoi(argv[3]);
  printf("Sender client: %d, port: %d\n", sender_client, sender_port);

  // first open the sequencer device for reading.
  int ret = midi_open(argv[1]);
  if (ret < 0) {
    if (ret == -1) {
      fprintf(stderr, "Error: cannot open %s\n", argv[1]);
    } else if (ret == -2) {
      fprintf(stderr, "Error: failed to set client name\n");
    } else {
      fprintf(stderr, "Error: create port error code %d\n", ret + 3);
    }
    exit(2);
  }

  printf("Opened %s:%d\n", argv[1], in_port);

  ret = capture_keyboard(sender_client, sender_port);
  if (ret < 0) {
    fprintf(stderr, "Error: failed connecting from sender\n");
  }

  // now just wait around for MIDI bytes to arrive and print them to screen.
  while (1) {
    snd_seq_event_t *ev = midi_read();
    if (ev == NULL) {
        continue;
    }
    midi_process(ev);
  }

  return 0;
}
