#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>
#include <math.h>

// ws281x References
#include "ws2811.h"

// aubio References
#include <aubio/aubio.h>
#include <aubio/mathutils.h>

// libsoundio References
#include <soundio/soundio.h>

#define TARGET_FREQ             WS2811_TARGET_FREQ
#define LED_1_GPIO_PIN          10
#define DMA                     10
#define STRIP_TYPE              WS2811_STRIP_GRB		// WS2812/SK6812RGB integrated chip+leds

#define LED_COUNT               300
#define CHANNELS                1

struct RGB
{
	unsigned char R;
	unsigned char G;
	unsigned char B;
};

struct HSV
{
	double H;
	double S;
	double V;
};

static uint8_t running = 1;

uint64_t previous_time;

int pre_offsets[CHANNELS] = { 0 };
int post_offsets[CHANNELS] = { 37 };
int midpoint_offsets[CHANNELS] = { -1 };

int led_counts[CHANNELS];
int led_origins[CHANNELS];
int led_bounds[CHANNELS][2];

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        {
            .gpionum = LED_1_GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
        },
    },
};

ws2811_led_t matrix[CHANNELS][LED_COUNT];
struct HSV state[CHANNELS][LED_COUNT];

// aubio

aubio_pitch_t *o;
aubio_filterbank_t *fb;
aubio_pvoc_t *pv;
aubio_fft_t *fft;
fvec_t *pitch_buf;
fvec_t *energy_mag_acc;
fvec_t *local_dim_buf;
fvec_t *buffer;
fvec_t *out_filters;
fvec_t *pitch_acc;
cvec_t *fftgrain;
fmat_t *fb_coeffs;
const uint_t win_s = 512;
const uint_t hop_s = 256;
const uint_t n_filters = 40;
const uint_t n_pitch_acc = 400;
const uint_t n_energy_mag_acc = 100;
const uint_t n_local_dim = 1000;
const uint_t max_buffer_runs = 400;
uint_t samplerate;

static void read_callback(struct SoundIoInStream *instream, int frame_count_min, int frame_count_max)
{
    struct SoundIoChannelArea *areas;
    int err;

    int wasted_frames = frame_count_max % buffer->length;

    int itt = frame_count_max / buffer->length;

    if (itt > max_buffer_runs)
    {
        wasted_frames = wasted_frames + (itt - max_buffer_runs) * buffer->length;
        itt = max_buffer_runs;
    }

    // Instead of wasting these frames
    // Try to run them through aubio and stage them for output
    if (wasted_frames > 0)
    {
        if ((err = soundio_instream_begin_read(instream, &areas, &wasted_frames))) {
            fprintf(stderr, "begin read error: %s", soundio_strerror(err));
            exit(1);
        }

        if ((err = soundio_instream_end_read(instream))) {
            fprintf(stderr, "end read error: %s", soundio_strerror(err));
            exit(1);
        }
    }

    for (int i = 0; i < itt; i++){
        if ((err = soundio_instream_begin_read(instream, &areas, &buffer->length))) {
            fprintf(stderr, "begin read error: %s", soundio_strerror(err));
            exit(1);
        }

        if (!areas) {
            // Due to an overflow there is a hole. Fill the ring buffer with
            // silence for the size of the hole.
            // memset(write_ptr, 0, frame_count * instream->bytes_per_frame);
        } else {
            for (int frame = 0; frame < buffer->length; frame++) {
                memcpy(&(buffer->data[frame]), areas[0].ptr, instream->bytes_per_sample);
                areas[0].ptr += areas[0].step;
            }
        }

        if ((err = soundio_instream_end_read(instream))) {
            fprintf(stderr, "end read error: %s", soundio_strerror(err));
            exit(1);
        }

        aubio_pitch_do(o, buffer, pitch_buf);

        if (pitch_buf->data[0] != 0) {
            fvec_push(pitch_acc, pitch_buf->data[0]);
        }

        aubio_pvoc_do(pv, buffer, fftgrain);

        // Clear the buffer so that future memcpy does not ruin it
        fvec_zeros(buffer);

        uint_t n = 1;
        while (n) {
            aubio_filterbank_do(fb, fftgrain, out_filters);
            n--;
        }

        float energy_mag;
        float energy_res;

        out_filters->data[0] = out_filters->data[0] * 5;

        for (int i = 0; i < out_filters->length; i++)
        {
            energy_mag = energy_mag + out_filters->data[i] * out_filters->data[i];
        }

        energy_mag = sqrtf(energy_mag);
        energy_res = (energy_mag - 1);

        fvec_push(energy_mag_acc, energy_res);
        //fvec_push(local_dim_buf, energy_res);
    }

    // fb_coeffs = aubio_filterbank_get_coeffs(fb);

    // fmat_print(fb_coeffs);
    // fvec_print(out_filters);
}

static void overflow_callback(struct SoundIoInStream *instream)
{
    printf("overflow.\n");
    exit(1);
}

double min(double x, double y) {
    return (x < y) ? x : y;
}

double max(double x, double y) {
    return (x > y) ? x : y;
}

double map(double x, double in_min, double in_max, double out_min, double out_max)
{
    return max(out_min, min(out_max, (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min));
}

int rand_range(int min_n, int max_n)
{
    return rand() % (max_n - min_n + 1) + min_n;
}

uint32_t color(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 16) | (g << 8) | b;
}

struct RGB HSVToRGB(struct HSV hsv)
{
	double r = 0, g = 0, b = 0;

	if (hsv.S == 0)
	{
		r = hsv.V;
		g = hsv.V;
		b = hsv.V;
	}
	else
	{
		int i;
		double f, p, q, t;

        hsv.H = hsv.H * 360;

		if (hsv.H == 360)
			hsv.H = 0;
		else
			hsv.H = hsv.H / 60;

		i = (int)trunc(hsv.H);
		f = hsv.H - i;

		p = hsv.V * (1.0 - hsv.S);
		q = hsv.V * (1.0 - (hsv.S * f));
		t = hsv.V * (1.0 - (hsv.S * (1.0 - f)));

		switch (i)
		{
		case 0:
			r = hsv.V;
			g = t;
			b = p;
			break;

		case 1:
			r = q;
			g = hsv.V;
			b = p;
			break;

		case 2:
			r = p;
			g = hsv.V;
			b = t;
			break;

		case 3:
			r = p;
			g = q;
			b = hsv.V;
			break;

		case 4:
			r = t;
			g = p;
			b = hsv.V;
			break;

		default:
			r = hsv.V;
			g = p;
			b = q;
			break;
		}

	}

	struct RGB rgb;
	rgb.R = r * 255;
	rgb.G = g * 255;
	rgb.B = b * 255;

	return rgb;
}

uint32_t color_from_hsv(double h, double s, double v)
{
    struct HSV hsv = {h, s, v};
    struct RGB rgb = HSVToRGB(hsv);
    return color(rgb.R, rgb.G, rgb.B);
}

uint32_t color_from_hsv_struct(struct HSV hsv)
{
    struct RGB rgb = HSVToRGB(hsv);
    return color(rgb.R, rgb.G, rgb.B);
}

void set_led(int channel, int index, struct HSV color)
{
    state[channel][index + pre_offsets[channel]] = color;
}

void set_led_origin(int channel, int offset, struct HSV color)
{
    state[channel][led_origins[channel] + offset] = color;
}

void or_led(int channel, int index, struct HSV color)
{
    state[channel][index + pre_offsets[channel]].H = (state[channel][index + pre_offsets[channel]].H + color.H) / 2.0;
    state[channel][index + pre_offsets[channel]].S = (state[channel][index + pre_offsets[channel]].S + color.S) / 2.0;
    state[channel][index + pre_offsets[channel]].V = (state[channel][index + pre_offsets[channel]].V + color.V) / 2.0;
}

void or_led_origin(int channel, int offset, struct HSV color)
{
    state[channel][led_origins[channel] + offset].H = (state[channel][led_origins[channel] + offset].H + color.H) / 2.0;
    state[channel][led_origins[channel] + offset].S = (state[channel][led_origins[channel] + offset].S + color.S) / 2.0;
    state[channel][led_origins[channel] + offset].V = (state[channel][led_origins[channel] + offset].V + color.V) / 2.0;
}

void shift_origin(int channel, int shift_amount)
{
    for (int i = led_counts[channel] + pre_offsets[channel]; i >= led_origins[channel] + shift_amount; i--)
    {
        state[channel][i] = state[channel][i - shift_amount];
    }

    for (int i = pre_offsets[channel]; i <= led_origins[channel] - shift_amount; i++)
    {
        state[channel][i] = state[channel][i + shift_amount];
    }

    for (int i = led_origins[channel] - shift_amount + 1; i < led_origins[channel] + shift_amount; i++)
    {
        state[channel][i].V = 0;
    }
}

double get_elapsed()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    uint64_t current = (uint64_t)ts.tv_sec * 1000000000U + (uint64_t)ts.tv_nsec;

    if (previous_time == 0)
    {
        previous_time = current;
        return 0;
    }

    double result = ((double)(current - previous_time)) / 1000000000.0;

    previous_time = current;

    return result;
}

void matrix_render(void)
{
    int x, i;

    // Only render the parts we are seeing
    for (i = 0; i < CHANNELS; i++)
    {
        for (x = pre_offsets[i]; x < led_counts[i] + pre_offsets[i]; x++)
        {
            ledstring.channel[i].leds[x] = matrix[i][x];
        }
    }
}

void state_render(void)
{
    int x, i;

    // Only render the parts we are seeing
    for (i = 0; i < CHANNELS; i++)
    {
        for (x = pre_offsets[i]; x < led_counts[i] + pre_offsets[i]; x++)
        {
            ledstring.channel[i].leds[x] = color_from_hsv(state[i][x].H, state[i][x].S, state[i][x].V);
        }
    }
}

void state_clear(int channel)
{
    int x, i;

    if (channel == -1)
    {
        for (i = 0; i < CHANNELS; i++)
        {
            for (x = 0; x < LED_COUNT; x++)
            {
                state[i][x].V = 0;
            }
        }
    }
    else
    {
        for (x = 0; x < LED_COUNT; x++)
        {
            state[channel][x].V = 0;
        }
    }
}

static void ctrl_c_handler(int signum)
{
	(void)(signum);
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

struct SoundIoInStream *instream;
struct SoundIoDevice *selected_device;
struct SoundIo *soundio;

int init_audio(int argc, char *argv[])
{
    int err;

    printf("Setting up audio devices...\n");
    // Setup audio thread
    soundio = soundio_create();

    if (!soundio)
    {
        fprintf(stderr, "sio out of memory\n");
        return 1;
    }

    if (soundio_connect_backend(soundio, SoundIoBackendAlsa))
    {
        fprintf(stderr, "sio error connecting\n");
        return 1;
    }

    soundio_flush_events(soundio);

    int device_index = soundio_default_input_device_index(soundio);
    selected_device = soundio_get_input_device(soundio, device_index);
    if (!selected_device)
    {
        fprintf(stderr, "sio No input devices available.\n");
        return 1;
    }

    fprintf(stderr, "Device: %s\n", selected_device->name);

    if (selected_device->probe_error)
    {
        fprintf(stderr, "Unable to probe device: %s\n", soundio_strerror(selected_device->probe_error));
        return 1;
    }

    soundio_device_sort_channel_layouts(selected_device);

    samplerate = selected_device->sample_rates[0].max;

    instream = soundio_instream_create(selected_device);
    if (!instream)
    {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    instream->format = SoundIoFormatFloat32NE; // May need to change this if Aubio is not giving correct results (set to SoundIoFormatFloat32BE)
    instream->sample_rate = samplerate;
    instream->read_callback = read_callback;
    instream->overflow_callback = overflow_callback;
    instream->userdata = NULL;

    // Setup Aubio with the parameters from the hardware
    buffer = new_fvec(hop_s);
    pitch_buf = new_fvec(1);
    energy_mag_acc = new_fvec(n_energy_mag_acc);
    local_dim_buf = new_fvec(n_local_dim);
    out_filters = new_fvec(n_filters);
    pitch_acc = new_fvec(n_pitch_acc);
    fvec_zeros(pitch_acc);
    fftgrain = new_cvec(win_s);
    o = new_aubio_pitch ("default", win_s, hop_s, samplerate);
    pv = new_aubio_pvoc(win_s, hop_s);
    fb = new_aubio_filterbank(n_filters, win_s);
    fft = new_aubio_fft(win_s);
    aubio_filterbank_set_mel_coeffs_slaney(fb, samplerate);

    if ((err = soundio_instream_open(instream)))
    {
        fprintf(stderr, "unable to open input stream: %s", soundio_strerror(err));
        return 1;
    }

    if ((err = soundio_instream_start(instream)))
    {
        fprintf(stderr, "unable to start input device: %s", soundio_strerror(err));
        return 1;
    }
}

int init(int argc, char *argv[]) 
{
    // Setup random number seed
    srand(time(NULL));

    if (init_audio(argc, argv) > 0)
    {
        return 1;
    }

    printf("Starting handlers...\n");
    setup_handlers();

    printf("Calculating led parameters...\n");
    for (int i = 0; i < CHANNELS; i++)
    {
        led_counts[i] = LED_COUNT - pre_offsets[i] - post_offsets[i];
        led_origins[i] = led_counts[i] / 2 + midpoint_offsets[i] + pre_offsets[i];
        led_bounds[i][0] = led_counts[i] / 2 + midpoint_offsets[i];
        led_bounds[i][1] = led_counts[i] / 2 - midpoint_offsets[i] - 1;
    }

    printf("Starting threads...\n");
    // Run effect setup

    printf("LED initialization...\n");
    // Finish LED init
    if (ws2811_init(&ledstring) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed.\n");
        return 1;
    }

    return 0;
}

void dispose()
{
    printf("Disposing objects...\n");

    soundio_instream_destroy(instream);
    soundio_device_unref(selected_device);
    soundio_destroy(soundio);

    printf("Disposed soundio.\n");

    del_aubio_filterbank(fb);
    del_aubio_pitch(o);
    del_aubio_pvoc(pv);
    del_cvec(fftgrain);
    del_fvec(pitch_buf);
    del_fvec(buffer);
    del_fvec(local_dim_buf);
    del_fvec(out_filters);
    aubio_cleanup();

    printf("Disposed aubio.\n");

    state_clear(-1);
    state_render();

    ws2811_render(&ledstring);

    printf("Cleared LEDs.\n");

    ws2811_fini(&ledstring); // This throws a seg fault

    printf("Disposed LEDs.\n");
}

void anti_alias_matrix_origin(int channel, double frame)
{
    double shift = (frame - (int)frame);

    for (int i = led_counts[channel] + pre_offsets[channel]; i >= led_origins[channel]; i--)
    {
        struct HSV new_val = state[channel][i];
        struct HSV next_val = state[channel][i - 1];

        new_val.V = new_val.V * (1 - shift);

        new_val.H = (new_val.H * new_val.V + next_val.H * next_val.V * shift) / (new_val.V + next_val.V * shift);
        new_val.S = new_val.S + next_val.S * shift;
        new_val.V = new_val.V + next_val.V * shift;

        matrix[channel][i] = color_from_hsv_struct(new_val);
    }

    for (int i = pre_offsets[channel]; i <= led_origins[channel]; i++)
    {
        struct HSV new_val = state[channel][i];
        struct HSV next_val = state[channel][i + 1];

        new_val.V = new_val.V * (1 - shift);

        new_val.H = (new_val.H * new_val.V + next_val.H * next_val.V * shift) / (new_val.V + next_val.V * shift);
        new_val.S = new_val.S + next_val.S * shift;
        new_val.V = new_val.V + next_val.V * shift;

        matrix[channel][i] = color_from_hsv_struct(new_val);
    }
}

void decay_brightness_origin(int channel, double decay)
{
    for (int i = 0; i < led_counts[channel]; i++)
    {
        state[channel][i].V = state[channel][i].V * decay;
    }
}

void calibration(int channel)
{
    for (int i = 0; i < led_bounds[channel][0]; i++)
    {
        set_led_origin(channel, -i, (struct HSV){ .H = 0.5, .S = 1, .V = 0.1 });
    }

    for (int i = 0; i < led_bounds[channel][1]; i++)
    {
        set_led_origin(channel, i, (struct HSV){ .H = 0.75, .S = 1, .V = 0.1 });
    }

    set_led_origin(channel, 0, (struct HSV){ .H = 0, .S = 1, .V = 1 });
}

void random_sparkles(int channel, double frame, int frame_hops, int disp, float lightness)
{
    if (frame_hops > 0)
    {
        shift_origin(channel, frame_hops);
    }

    if (disp)
    {
        set_led_origin(channel, 0, (struct HSV){ .H = 0, .S = 0, .V = lightness });
    }
}

void music_flow(int channel, double frame, int frame_hops, float pitch_acc_mean, float lightness)
{
    if (frame_hops > 0)
    {
        shift_origin(channel, frame_hops);
    }

    if (pitch_acc_mean > 1)
    {
        set_led_origin(channel, 0, (struct HSV){ .H = map((int)(12 * log2f(pitch_acc_mean)) % 20, 0, 20, 0, 1), .S = 1, .V = lightness });
    }
}

void fractional_bar(int channel, double pos, double width, double hue, bool wrap)
{
    int upper_pixel = (int)(ceil(pos) + led_counts[channel]) % led_counts[channel];

    if (wrap || upper_pixel == (int)ceil(pos))
    {
        set_led(channel, upper_pixel, (struct HSV){ .H = hue, .S = 1, .V = pos - (int)pos });
    }

    int lower_pixel = (int)(floor(pos) - ceil(width) + led_counts[channel]) % led_counts[channel];

    if (wrap || lower_pixel == (int)(floor(pos) - width))
    {
        set_led(channel, lower_pixel, (struct HSV){ .H = hue, .S = 1, .V = 1 - (pos - (int)pos) });
    }

    for (int i = 0; i < floor(width); i++) {
        int index = ((int)floor(pos) - i + led_counts[channel]) % led_counts[channel];

        if (wrap || index == (int)floor(pos) - i)
        {
            set_led(channel, index, (struct HSV){ .H = hue, .S = 1, .V = 1 });
        }
    }
}

void christmas(int channel, double frame)
{
    for (int i = 0; i < led_counts[channel]; i++) {
        set_led(channel, i, (struct HSV){ .H = ((int)frame + i % 2 == 0 ? 0 : 0.3), .S = 1, .V = 1 });
    }
}

void ring(int channel, double frame, double hue)
{
    double pos = ((int)frame + led_counts[channel]) % led_counts[channel] + (frame - (int)frame);
    fractional_bar(channel, pos, 8, hue, true);
}

void ring_pair(int channel, double frame, double hue)
{
    ring(channel, frame, hue);
    ring(channel, -frame, 1 - hue);
}

void bouncer(int channel, double frame, int frame_hops)
{
    if (frame_hops > 0)
    {
        shift_origin(channel, frame_hops);
    }

    if (frame > 0 && ((int)(frame / 15)) % 12 == 0) {
        set_led_origin(channel, 0, (struct HSV){ .H = map((int)(frame / 15) % 21, 0, 21, 0, 1), .S = 1, .V = 1 });
    }
}

int main(int argc, char *argv[])
{
    printf("Initializing...\n");
    ws2811_return_t ret;

    int err;
    if ((err = init(argc, argv)) > 0) {
        printf("Error: %d", err);
        exit(1);
    }

    printf("Initialization complete.\n");
    printf("Running animation loop.\n");
    double elapsed_time;
    double frame[CHANNELS];
    double animation_speed = 1.0;
    double channel_speeds[CHANNELS];
    int frame_hops[CHANNELS];

    for (int channel = 0; channel < CHANNELS; channel++)
    {
        channel_speeds[channel] = ((double)led_counts[channel] / (double)led_counts[0]) * animation_speed;
        frame[channel] = 0;
        frame_hops[channel] = 0;
    }

    elapsed_time = get_elapsed();

    while (running)
    {
        elapsed_time = get_elapsed();

        // Consider making this a weighted mean by time
        float energy_mag_acc_mean = fvec_mean(energy_mag_acc);
        float energy_local_dim_mean = fvec_mean(local_dim_buf);

        double lightness = map(energy_mag_acc_mean / (energy_local_dim_mean + 0.001), 0.0005, 1.5, 0, 1);

        float pitch_acc_mean = fvec_mean(pitch_acc);

        // Modify matrix based on effect
        for (int channel = 0; channel < CHANNELS; channel++)
        {
            // Common calculations done per channel
            double channel_elapsed = elapsed_time * channel_speeds[channel];

            frame_hops[channel] = floor(frame[channel] + channel_elapsed) - floor(frame[channel]);

            frame[channel] = frame[channel] + channel_elapsed;

            if (frame[channel] >= led_counts[channel])
            {
                frame[channel] = frame[channel] - led_counts[channel];
            }

            state_clear(channel);
            //ring_pair(channel, frame[channel], map((int)frame[channel] % 360, 0, 360, 0, 1));
            christmas(channel, frame[channel]);

            // Render a specific effect

            //calibration(channel);
            //random_sparkles(channel, frame[channel], frame_hops[channel], disp, lightness);
            //music_flow(channel, frame[channel], frame_hops[channel], pitch_acc_mean, lightness);

            // Run anti-aliasing and origin dimming
            //anti_alias_matrix_origin(channel, frame[channel]);
            //decay_brightness_origin(channel, 0.985);
        }

        //matrix_render();
        state_render();
        if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
        {
            fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
            break;
        }
    }

    dispose();

    printf("\n");
    return ret;
}
