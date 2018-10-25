import pyaudio
import numpy as np
from aubio import tempo, pitch, pvoc, filterbank
import _rpi_ws281x as ws
import threading
import time
import colorsys
import math
from collections import deque
import sys, getopt

from neopixel import *

LED_COUNT = 300

LED_PRE_OFFSET = [ 17, 18 ]
LED_POST_OFFSET = [16, 14]
LED_MIDPOINT_SHIFT = [0, 1]

LED_1_COUNT      = 300
LED_1_PIN        = 10
LED_1_FREQ_HZ    = 800000
LED_1_DMA        = 10
LED_1_BRIGHTNESS = 255
LED_1_INVERT     = 0
LED_1_CHANNEL    = 0
LED_1_STRIP      = ws.WS2811_STRIP_GRB

LED_2_COUNT      = 300
LED_2_PIN        = 13
LED_2_FREQ_HZ    = 800000
LED_2_DMA        = 11
LED_2_BRIGHTNESS = 255
LED_2_INVERT     = 0
LED_2_CHANNEL    = 1
LED_2_STRIP      = ws.WS2811_STRIP_GRB

def Map(x, in_min, in_max, out_min, out_max):
    return max(out_min, min(out_max, (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min))

def hsv2rgb(h, s, v):
    return tuple(int(round(i * 255)) for i in colorsys.hsv_to_rgb(h, s, v))

def color_from_hsv(h, s, v):
    rgb = hsv2rgb(h, s, v)
    return Color(rgb[0], rgb[1], rgb[2])

def clear(state):
    for i in range(len(state)):
        state[i] = 0


leds = [0, 1]

leds[0] = Adafruit_NeoPixel(LED_1_COUNT, LED_1_PIN, LED_1_FREQ_HZ, LED_1_DMA, LED_1_INVERT, LED_1_BRIGHTNESS, LED_1_CHANNEL, LED_1_STRIP)
leds[1] = Adafruit_NeoPixel(LED_2_COUNT, LED_2_PIN, LED_2_FREQ_HZ, LED_2_DMA, LED_2_INVERT, LED_2_BRIGHTNESS, LED_2_CHANNEL, LED_2_STRIP)

leds[0].begin()
leds[1].begin()


def set_led(index, color, strip):
    leds[strip].setPixelColor(index + LED_PRE_OFFSET[strip], color)

def set_led_origin(state, offset, color, strip):
    state[offset + LED_MIDPOINT_SHIFT[strip] + get_actual_len(strip) // 2] = color

def get_actual_len(strip):
    return LED_COUNT - LED_PRE_OFFSET[strip] - LED_POST_OFFSET[strip]

def get_origin_bounds(strip):
    return [get_actual_len(strip) // 2 + LED_MIDPOINT_SHIFT[strip], get_actual_len(strip) // 2 - LED_MIDPOINT_SHIFT[strip] - 1]

def get_origin(strip):
    return get_actual_len(strip) // 2 + LED_MIDPOINT_SHIFT[strip]

def render():
    leds[0].show()
    leds[1].show()

def get_len(state):
    return len(state)

def draw_fractional_bar_abs(states, strip_start, strip_end, pos_start, pos_end, hue):
    pass

def draw_fractional_bar(state, pos, width, hue, wrap):
    upper_pixel = (math.ceil(pos)) % (get_len(state))
    if wrap or upper_pixel == (math.ceil(pos)):
        state[upper_pixel] = state[upper_pixel] | color_from_hsv(hue, 1, pos - int(pos))

    lower_pixel = (math.floor(pos) - math.ceil(width)) % get_len(state)
    if wrap or lower_pixel == (math.floor(pos) - width):
        state[lower_pixel] = state[lower_pixel] | color_from_hsv(hue, 1, 1 - (pos - int(pos)))

    for i in range(math.floor(width)):
        index = (math.floor(pos) - i) % get_len(state)
        if wrap or index == (math.floor(pos) - i):
            state[index] = state[index] | color_from_hsv(hue, 1, 1)

def ring(state, frame, hue):
    pos = frame % get_len(state)
    draw_fractional_bar(state, pos, 3, hue, True)

def ring_pair(state, frame):
    clear(state)
    ring(state, frame, 0.083)
    ring(state, -frame, 0.417)

def double_chaser(state, frame):
    clear(state)
    frame = frame * 2
    ring(state, frame, 0)
    ring(state, frame + get_len(state) / 2, 0.417)

def wave1(state, frame):
    hue_offset = -(frame % LED_COUNT) / LED_COUNT

    for i in range(get_len(state)):
        hue = (hue_offset + Map(i, 0, get_len(state), 0, 1)) % 1
        state[i] = color_from_hsv(hue, 1, 0.5)

def theater(state, frame):
    for i in range(get_len(state)):
        if (i + (frame % 2)) % 2 < 1:
            state[i] = Color(0, 0, 0)
        else:
            state[i] = Color(50, 50, 50)

pos = 0
count = 0
def around(states, frame, animation_speed, hue, brightness):
    global count
    global pos

    seq =       [-1, 1, 1, -1, 1, -1, -1, 1]
    pos_strip = [0, 0, 1, 1, 0, 0, 1, 1]
    count_dir = [-1, 0, 1, 0, 1, 0, -1, 0]

    count = count + seq[pos] * animation_speed

    width = 3

    hue_offset = (frame % LED_COUNT) / LED_COUNT

    for y in range(len(states)):
        clear(states[y])

    bounds = get_origin_bounds(pos_strip[pos])
    next_bounds = get_origin_bounds(pos_strip[(pos + 1) % 8])

    if count >= bounds[1]:
        pos = (pos + 1) % 8
        count = bounds[1]
    elif count <= -bounds[0]:
        pos = (pos + 1) % 8
        count = -bounds[0]
    
    if count_dir[pos] == 0:
        if seq[pos] == -1:
            if count <= 0:
                pos = (pos + 1) % 8
                count = 0
        elif seq[pos] == 1:
            if count >= 0:
                pos = (pos + 1) % 8
                count = 0

    for y in range(len(states)):
        if pos_strip[pos] == y:
            color = color_from_hsv(hue_offset, 1, 0.5)

            if brightness != -1:
                color = color_from_hsv(hue_offset, 1, brightness % 1)

            set_led_origin(states[y], int(count), color, y)


def run_effect(effect_name, animation_speed, hue, brightness):
    print('Running efffect: ' + effect_name)

    frame = 0
    state = [[0] * ((LED_1_COUNT - LED_PRE_OFFSET[0] - LED_POST_OFFSET[0]) // 1), [0] * ((LED_2_COUNT - LED_PRE_OFFSET[1] - LED_POST_OFFSET[1]) // 1)]

    while True:
        if effect_name == 'ring_pair':
            ring_pair(state[0], frame)
            ring_pair(state[1], frame)
        elif effect_name == 'double_chaser':
            double_chaser(state[0], frame)
            double_chaser(state[1], frame)
        elif effect_name == 'wave1':
            wave1(state[0], frame)
            wave1(state[1], frame)
        elif effect_name == 'theater':
            theater(state[0], frame)
            theater(state[1], frame)
        elif effect_name == 'power_test':
            for i in range(len(state[0])):
                state[0][i] = Color(60, 60, 60)
            for i in range(len(state[1])):
                state[1][i] = Color(60, 60, 60)
        elif effect_name == 'around':
            around(state, frame, animation_speed, hue, brightness)
        elif effect_name == 'midpoint':
            for y in range(len(state)):
                for i in range(get_origin_bounds(y)[0]):
                    set_led_origin(state[y], -i, Color(0, 20, 0), y)
                for i in range(get_origin_bounds(y)[1]):
                    set_led_origin(state[y], i, Color(0, 0, 20), y)
                
                set_led_origin(state[y], 0, Color(255, 0, 0), y)

        frame = frame + animation_speed

        if len(state[0]) == (LED_COUNT - LED_PRE_OFFSET[0] - LED_POST_OFFSET[0]):
            for y in range(len(state)):
                for i in range(len(state[y])):
                    set_led(i, state[y][i], y)
        elif len(state[0]) == (LED_COUNT - LED_PRE_OFFSET[0] - LED_POST_OFFSET[0]) // 2:
            for y in range(len(state)):
                for i in range(len(state[y])):
                    set_led(len(state[y]) + i, state[y][i], y)
                    set_led(len(state[y]) - i - 1, state[y][i], y)

        render()

has_beat = False
pooled_energies = []
pooled_frequency = deque([])
cond = threading.Condition()
frame_count = 0

frequency_kernel_size = 5


def process_sound(cond):
    global has_beat
    global pooled_energies
    global pooled_frequency
    global frame_count
    global leds
    global channel
    global frequency_kernel_size

    print('Thread Started.')

    offset = 0
    state = [0] * LED_COUNT
    output = [[0] * ((LED_1_COUNT - LED_PRE_OFFSET[0] - LED_POST_OFFSET[0]) // 1), [0] * ((LED_2_COUNT - LED_PRE_OFFSET[1] - LED_POST_OFFSET[1]) // 1)]
    hue_offset = 0
    beat_boost = 0
    beat_boost_amount = 4
    beat_boost_decay = 0.7

    # Use exp of energy to lightness instead of map

    try:
        while True:

            # Minimum frame delay
            # time.sleep(0.02)

            frame_mult = 1
            seg_count = 1

            with cond:
                cond.wait()

                is_beat = has_beat

                if has_beat:
                    has_beat = False

                seg_count = 1#frame_count * frame_mult
                frame_count = 0

                frequency = np.mean(np.array(pooled_frequency))

                while len(pooled_frequency) > frequency_kernel_size:
                    pooled_frequency.popleft()

                energies = np.mean(np.array(pooled_energies), axis=0)
                pooled_energies = []

            color = Color(0, 0, 0)
            energy_sum = np.sum(energies)

            energies[0] = energies[0] * 4

            for i in range(15, 20):
                energies[i] = energies[i] * 2

            for i in range(20, 40):
                energies[i] = energies[i] * 4

            energy_max_bin = np.argmax(energies)

            local_energy_sum = energies[energy_max_bin]

            hue_offset = (hue_offset + 0.001) % 1

            beat_boost = beat_boost * beat_boost_decay

            # TODO: Add beat with decaying brightness input

            if frequency > 0:
                # Print out the information (for debugging)

                if is_beat and energy_sum > 0.01:
                    pass
                    # beat_boost = beat_boost_amount
                    # print('---------- Beat ----------')

                # print("%.2f" % energy_sum)
                # print(frequency)

                # Modify LEDs

                # At about 160Hz the frequency analysis stops working or the energy_sum does not really account for the sound being played
                # Will need to do a different regional energy that finds the peak or at least the smoothed peak of the energy graph and checks that energy
                # Then apply this to lightness and hopefully get rid of frequency altogether
                # Frequency upper bound currently is 2500

                lightness = Map((energy_sum / 40 + local_energy_sum) * max(1, beat_boost), 0.05, 4, 0, 1)

                rgb = hsv2rgb((hue_offset + Map(12 * math.log(frequency, 2) % 50, 0, 50, 0, 1)) % 1, 1, lightness)
                color = Color(rgb[0], rgb[1], rgb[2])

                # print(str(rgb[0]) + ':' + str(rgb[1]) + ':' + str(rgb[2]))
                # print(beat_boost)

                # Insert colored segments

            seg_count = seg_count
            for y in range(len(output)):
                for i in range(seg_count):
                    state.insert(0, color)
            state = state[:len(state) - seg_count]

            # Push state to current
            for y in range(len(output)):
                bounds = get_origin_bounds(y)
                for i in range(get_len(output[y])):
                    if i > 0:
                        if i < bounds[0]:
                            set_led_origin(output[y], -i, state[i], y)
                        if i < bounds[1]:
                            set_led_origin(output[y], i, state[i], y)
                    else:
                        set_led_origin(output[y], 0, state[i], y)

            for y in range(len(output)):
                for i in range(get_len(output[y])):
                    set_led(i, output[y][i], y)
            
            render()
    finally:
        try:
            for y in range(len(output)):
                for i in range(LED_COUNT):
                    set_led(i, Color(0, 0, 0), y)
            
            render()
        finally:
            pass

def start_processing_thread():
    thread = threading.Thread(target=process_sound, args=(cond,))
    thread.daemon = True
    thread.start()


def listen():
    global pitch
    global has_beat
    global frame_count
    global pooled_energies
    global pooled_frequency

    p = pyaudio.PyAudio()

    hop_s = 512 # hop size
    pyaudio_format = pyaudio.paFloat32
    n_channels = 1
    samplerate = 44100
    stream = p.open(format=pyaudio_format,
                    channels=n_channels,
                    rate=samplerate,
                    input=True,
                    frames_per_buffer=hop_s)

    #outstream = p.open(format=pyaudio_format,
    #                    channels=n_channels,
    #                    rate=samplerate,
    #                    output=True,
    #                    frames_per_buffer=hop_s)

    tolerance = 0.8
    win_s = 1024 # fft size
    pitch_o = pitch("default", win_s, hop_s, samplerate)
    pitch_o.set_unit("Hz")
    pitch_o.set_tolerance(tolerance)
    tempo_o = tempo("specdiff", win_s, hop_s, samplerate)
    pv = pvoc(win_s, hop_s)
    f = filterbank(40, win_s)
    f.set_mel_coeffs_slaney(samplerate)

    energies = np.zeros((40,))

    beats = []

    print("*** starting recording")
    while True:
        try:
            audiobuffer = stream.read(hop_s, exception_on_overflow=False)
            signal = np.fromstring(audiobuffer, dtype=np.float32)

            is_beat = tempo_o(signal)

            pitch = pitch_o(signal)[0]
            fftgrain = pv(signal)
            new_energies = f(fftgrain)

            with cond:
                if is_beat and not has_beat:
                    has_beat = True

                frame_count = frame_count + 1
                pooled_frequency.append(pitch)
                pooled_energies.append(new_energies)
                cond.notify_all()

            # outstream.write(audiobuffer)
        except KeyboardInterrupt:
            print("*** Ctrl+C pressed, exiting")
            break

    print("*** done recording")
    stream.stop_stream()
    stream.close()
    p.terminate()


try:
    opts, args = getopt.getopt(sys.argv[1:], 'hs:ae:c:b:', ['effect=', 'help', 'speed=', 'color=', 'brightness='])
except getopt.GetoptError:
    print('main.py (-a|-e <effect name>) [-s <effect speed>]')

try:
    speed = 1
    hue = -1
    brightness = -1
    for opt, arg in opts:
        if opt in ('-s', '--speed'):
            speed = float(arg)
        elif opt in ('-c', '--color'):
            hue = float(arg)
        elif opt in ('-b', '--brightness'):
            brightness = float(arg)

    for opt, arg in opts:
        if opt in ('-h', '--help'):
            print('main.py (-a|-e <effect name>) [-s <effect speed>]')
        elif opt == '-a':
            start_processing_thread()
            listen()
        elif opt in ('-e', '--effect'):
            run_effect(arg, speed, hue, brightness)
        else:
            print('main.py [-a|-e <effect name>]')
finally:
    try:
        for y in range(2):
            for i in range(LED_COUNT):
                set_led(i, Color(0, 0, 0), y)
        
        render()
    finally:
        pass
