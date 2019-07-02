#include "jackrecorder.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <pthread.h>
#include <sys/time.h>
#include <ctime>

using std::cerr;
using std::endl;

unsigned int nports = 0;
jack_port_t **ports = NULL;
jack_default_audio_sample_t **in = NULL;
jack_nframes_t nframes = 0;
const size_t sample_size = sizeof(jack_default_audio_sample_t);

/* Synchronization between process thread and disk thread. */
#define DEFAULT_RB_SIZE 16384		/* ringbuffer size in frames */
jack_ringbuffer_t *rb = NULL;
pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  data_ready = PTHREAD_COND_INITIALIZER;
long overruns = 0;
jack_thread_info_t thread_info;
jack_client_t *client;

void* disk_thread (void *arg)
{
	jack_thread_info_t *info = (jack_thread_info_t *) arg;
	static jack_nframes_t total_captured = 0;
	jack_nframes_t samples_per_frame = info->channels;
	size_t bytes_per_frame = samples_per_frame * sample_size;
	char *framebuf = (char*)malloc (bytes_per_frame);

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock (&disk_thread_lock);

	info->status = 0;
	std::vector<double> buffer;
	double min = std::numeric_limits<double>::max();
	double max = std::numeric_limits<double>::min();

	while (1) {

		/* Write the data one frame at a time.  This is
		 * inefficient, but makes things simpler. */
		while (info->can_capture &&
		       (jack_ringbuffer_read_space (rb) >= bytes_per_frame)) {

			jack_ringbuffer_read (rb, framebuf, bytes_per_frame);
			int32_t frame = 0;
			for(size_t i = 0; i < bytes_per_frame; ++i) {
				int32_t shifted = ((int32_t)framebuf[i]) << (i*8);
 				frame = frame | shifted;
			}

			buffer.push_back((double)frame);

//			std::cerr << min << '\t' << max << std::endl;
//			std::cerr << ((double)frame / std::numeric_limits<int32_t>::max()) * 255.0 << std::endl;
			if(buffer.size() >= 1024) {
				thread_info.callback(buffer);
				buffer.clear();

				if (++total_captured >= info->duration) {
					printf ("disk thread finished\n");
					goto done;
				}
			}
		}

		/* wait until process() signals more data */
		pthread_cond_wait (&data_ready, &disk_thread_lock);
	}

 done:
	pthread_mutex_unlock (&disk_thread_lock);
	free (framebuf);
	return 0;
}

int process(jack_nframes_t nframes, void *arg)
{
	int chn;
	size_t i;
	jack_thread_info_t *info = (jack_thread_info_t *) arg;

	/* Do nothing until we're ready to begin. */
	if ((!info->can_process) || (!info->can_capture))
		return 0;

	for (chn = 0; chn < nports; chn++)
		in[chn] = (jack_default_audio_sample_t*)jack_port_get_buffer (ports[chn], nframes);

	/* Sndfile requires interleaved data.  It is simpler here to
	 * just queue interleaved samples to a single ringbuffer. */
	for (i = 0; i < nframes; i++) {
		for (chn = 0; chn < nports; chn++) {
			if (jack_ringbuffer_write (rb, (char *) (in[chn]+i),
					      sample_size)
			    < sample_size)
				overruns++;
		}
	}

	/* Tell the disk thread there is work to do.  If it is already
	 * running, the lock will not be available.  We can't wait
	 * here in the process() thread, but we don't need to signal
	 * in that case, because the disk thread will read all the
	 * data queued before waiting again. */
	if (pthread_mutex_trylock (&disk_thread_lock) == 0) {
	    pthread_cond_signal (&data_ready);
	    pthread_mutex_unlock (&disk_thread_lock);
	}

	return 0;
}

void
JackRecorder::jack_shutdown (void *arg)
{
	fprintf (stderr, "JACK shutdown\n");
	// exit (0);
	abort();
}

size_t JackRecorder::getSampleRate() {
	return jack_get_sample_rate (thread_info.client);
}
void
JackRecorder::setup_disk_thread (jack_thread_info_t *info)
{

	jack_nframes_t samplerate = jack_get_sample_rate (info->client);
	unsigned int channels = info->channels;



	if (info->duration == 0) {
		info->duration = JACK_MAX_FRAMES;
	} else {
		info->duration *= samplerate;
	}

	info->can_capture = 0;

	pthread_create (&info->thread_id, NULL, disk_thread, info);
}

void
JackRecorder::run_disk_thread (jack_thread_info_t *info)
{
	info->can_capture = 1;
	pthread_join (info->thread_id, NULL);
	if (overruns > 0) {
		fprintf (stderr,
			 "jackrec failed with %ld overruns.\n", overruns);
		fprintf (stderr, " try a bigger buffer than -B %"
			 PRIu32 ".\n", info->rb_size);
		info->status = EPIPE;
	}
}

void
JackRecorder::setup_ports (int sources, char *source_names[], jack_thread_info_t *info)
{
	unsigned int i;
	size_t in_size;

	/* Allocate data structures that depend on the number of ports. */
	nports = sources;
	ports = (jack_port_t **) malloc (sizeof (jack_port_t *) * nports);
	in_size =  nports * sizeof (jack_default_audio_sample_t *);
	in = (jack_default_audio_sample_t **) malloc (in_size);
	rb = jack_ringbuffer_create (nports * sample_size * info->rb_size);

	/* When JACK is running realtime, jack_activate() will have
	 * called mlockall() to lock our pages into memory.  But, we
	 * still need to touch any newly allocated pages before
	 * process() starts using them.  Otherwise, a page fault could
	 * create a delay that would force JACK to shut us down. */
	memset(in, 0, in_size);
	memset(rb->buf, 0, rb->size);

	for (i = 0; i < nports; i++) {
		char name[64];

		sprintf (name, "input%d", i+1);

		if ((ports[i] = jack_port_register (info->client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)) == 0) {
			fprintf (stderr, "cannot register input port \"%s\"!\n", name);
			jack_client_close (info->client);
			exit (1);
		}
	}

	for (i = 0; i < nports; i++) {
		if (jack_connect (info->client, source_names[i], jack_port_name (ports[i]))) {
			fprintf (stderr, "cannot connect input port %s to %s\n", jack_port_name (ports[i]), source_names[i]);
			jack_client_close (info->client);
			exit (1);
		}
	}

	info->can_process = 1;		/* process() can start, now */
}


JackRecorder::JackRecorder(const std::string& name) {
	memset(&thread_info, 0, sizeof (thread_info));
	thread_info.rb_size = DEFAULT_RB_SIZE;

	if ((client = jack_client_open (name.c_str(), JackNullOption, NULL)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		exit (1);
	}

	  thread_info.client = client;
		thread_info.channels = 1;
		thread_info.can_process = 0;

		setup_disk_thread (&thread_info);

		jack_set_process_callback (client, process, &thread_info);

		if (jack_activate (client)) {
			fprintf (stderr, "cannot activate client");
		}
}

void JackRecorder::setCallback(JackRecorderCallback& callback) {
	thread_info.callback = callback;
}

JackRecorder::~JackRecorder() {
	jack_client_close (client);
	jack_ringbuffer_free (rb);
}


void JackRecorder::capture(bool detach) {
	char* sources[] = {"system:capture_1"};
	setup_ports (1, sources, &thread_info);
	run_disk_thread (&thread_info);
}

