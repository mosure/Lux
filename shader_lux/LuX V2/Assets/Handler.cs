using Assets;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Audio; // required for dealing with audiomixers
using UnityEngine.Networking;

[RequireComponent(typeof(AudioSource))]
public class Handler : MonoBehaviour
{
    Renderer rend;

    public float Sensitivity = 1;

    public bool NoMicrophone = false;

    public bool IsWorking = true;
    private bool _lastValueOfIsWorking;

    public bool RealtimeOutput = false;
    private bool _lastValueOfRaltimeOutput;

    AudioSource _audioSource;

    // For Non-Microphone Use
    private AudioClip AudioClip;

    private const int NUM_SAMPLES = 1024;

    private const float refValue = 0.1f;
    public float threshold = 0.005f;

    public float PitchMoveRate = 0f;
    public float LoudnessDecayRate = 0.9f;
    private float LoudnessAccumulator = 0;
    private float PitchAccumulator = 0;

    private float[] samples = new float[NUM_SAMPLES];
    private float[] curSpectrum = new float[NUM_SAMPLES];

    public float pitch = 0;
    public float midi_num = 0;
    public float normalized_midi = 0;
    public float rmsValue = 0;
    public float dbValue = 0;

#if !UNITY_WEBGL
    private const int RING_COUNT = 256;
#else
    private const int RING_COUNT = 128;
#endif

    private int _ArrayIndex = 0;
    private float[] _LoudnessArray = new float[RING_COUNT];
    private float[] _PitchArray = new float[RING_COUNT];

    // make an audio mixer from the "create" menu, then drag it into the public field on this script.
    // double click the audio mixer and next to the "groups" section, click the "+" icon to add a 
    // child to the master group, rename it to "microphone".  Then in the audio source, in the "output" option, 
    // select this child of the master you have just created.
    // go back to the audiomixer inspector window, and click the "microphone" you just created, then in the 
    // inspector window, right click "Volume" and select "Expose Volume (of Microphone)" to script,
    // then back in the audiomixer window, in the corner click "Exposed Parameters", click on the "MyExposedParameter"
    // and rename it to "Volume"
    public AudioMixer masterMixer;

    void Start()
    {
        // yield return Application.RequestUserAuthorization(UserAuthorization.Microphone);

        InitializePlaneSize();

        _audioSource = GetComponent<AudioSource>();

#if !UNITY_WEBGL
        Application.targetFrameRate = 120;
#else
        StartCoroutine(GetAudioClip());
#endif

        rend = GetComponent<Renderer>();

        rend.material.shader = Shader.Find("ReactiveImageShader");


        if (IsWorking)
        {
            WorkStart();
        }

        DisableSound(RealtimeOutput);

        Cursor.visible = false;
    }

    void InitializePlaneSize()
    {
        Camera cam = Camera.main;

        float pos = (cam.nearClipPlane + 0.01f);

        transform.position = cam.transform.position + cam.transform.forward * pos;

        float h = Mathf.Tan(cam.fieldOfView * Mathf.Deg2Rad * 0.5f) * pos * 2f;

        transform.localScale = new Vector3(h * cam.aspect, h, 1f);
    }

    void Update()
    {
        CheckIfIsWorkingChanged();
        CheckIfRealtimeOutputChanged();

        // TODO: Calculate decay rates and number of rings based on delta time and average frame rate
        if (_audioSource != null && _audioSource.isPlaying)
        {
            AnalyzeSound();

            SetParameters();
        }
    }

    void SetParameters()
    {
        if (LoudnessAccumulator < rmsValue)
        {
            LoudnessAccumulator = rmsValue;
        }

        LoudnessAccumulator = LoudnessAccumulator * LoudnessDecayRate;

        _LoudnessArray[_ArrayIndex] = LoudnessAccumulator * Sensitivity;

        if (pitch > 20)
        {
            midi_num = 12 * Mathf.Log(pitch / 440, 2) + 69;
            normalized_midi = Mathf.Clamp01((midi_num - 21) / 105);

            PitchAccumulator = Mathf.Lerp(PitchAccumulator, normalized_midi, PitchMoveRate);
        }

        _PitchArray[_ArrayIndex] = PitchAccumulator;

        rend.material.SetInt("_ArrayIndex", _ArrayIndex);
        rend.material.SetFloatArray("_LoudnessArray", _LoudnessArray);
        rend.material.SetFloatArray("_PitchArray", _PitchArray);

        _ArrayIndex = (_ArrayIndex + 1) % RING_COUNT;
    }

    void AnalyzeSound()
    {
        _audioSource.GetOutputData(samples, 0); // fill array with samples
        int i;
        float sum = 0;
        for (i = 0; i < NUM_SAMPLES; i++)
        {
            sum += samples[i] * samples[i]; // sum squared samples
        }

        rmsValue = Mathf.Sqrt(sum / NUM_SAMPLES); // rms = square root of average
        dbValue = 20 * Mathf.Log10(rmsValue / refValue); // calculate dB
        if (dbValue < -160) dbValue = -160; // clamp it to -160dB min
                                            // get sound spectrum
        _audioSource.GetSpectrumData(curSpectrum, 0, FFTWindow.BlackmanHarris);
        float maxV = 0;
        int maxN = 0;
        for (i = 0; i < NUM_SAMPLES; i++)
        { // find max 
            if (curSpectrum[i] > maxV && curSpectrum[i] > threshold)
            {
                maxV = curSpectrum[i];
                maxN = i; // maxN is the index of max
            }
        }
        float freqN = maxN; // pass the index to a float variable
        if (maxN > 0 && maxN < NUM_SAMPLES - 1)
        { // interpolate index using neighbours
            var dL = curSpectrum[maxN - 1] / curSpectrum[maxN];
            var dR = curSpectrum[maxN + 1] / curSpectrum[maxN];
            freqN += 0.5f * (dR * dR - dL * dL);
        }

        pitch = freqN * (AudioSettings.outputSampleRate / 2) / NUM_SAMPLES; // convert index to frequency
    }

    //controls whether the volume is on or off, use "off" for mic input (dont want to hear your own voice input!) 
    //and "on" for music input
    public void DisableSound(bool SoundOn)
    {
        float volume = 0;

        if (SoundOn)
        {
            volume = 0.0f;
        }
        else
        {
            volume = -80.0f;
        }

        masterMixer.SetFloat("Volume", volume);
    }

    public void CheckIfIsWorkingChanged()
    {
        if (_lastValueOfIsWorking != IsWorking)
        {
            if (IsWorking)
            {
                WorkStart();
            }
            else
            {
                WorkStop();
            }
        }

        _lastValueOfIsWorking = IsWorking;
    }

    public void CheckIfRealtimeOutputChanged()
    {
        if (_lastValueOfRaltimeOutput != RealtimeOutput)
        {
            DisableSound(RealtimeOutput);
        }

        _lastValueOfRaltimeOutput = RealtimeOutput;
    }

#if !UNITY_WEBGL
    private void WorkStart()
    {
        if (Microphone.devices.Length > 0)
        {
            _audioSource.clip = Microphone.Start(null, true, 10, AudioSettings.outputSampleRate);
            _audioSource.loop = true;
            while (!(Microphone.GetPosition(null) > 0))
            {
                _audioSource.Play();
            }
            NoMicrophone = false;
        }
        else
        {
            NoMicrophone = true;
        }
    }

    private void WorkStop()
    {
        if (Microphone.devices.Length > 0)
        {
            Microphone.End(null);
            NoMicrophone = false;
        }
        else
        {
            NoMicrophone = true;
        }
    }
#else
    private void WorkStart()
    {
        if (AudioClip != null)
        {
            _audioSource.clip = AudioClip;
            _audioSource.loop = true;
            _audioSource.Play();
        }
    }

    private void WorkStop()
    {

    }

    IEnumerator GetAudioClip()
    {
        string url = URLParameters.GetSearchParameters().GetString("url", @"https://mitchell.mosure.me/assets/Lux/bensound-dubstep.wav");

        Debug.Log(url);

        if (url != null)
        {
            using (UnityWebRequest www = UnityWebRequestMultimedia.GetAudioClip(url, AudioType.WAV))
            {
                yield return www.SendWebRequest();

                if (www.isNetworkError)
                {
                    Debug.Log(www.error);
                }
                else
                {
                    AudioClip = DownloadHandlerAudioClip.GetContent(www);
                }
            }

            WorkStart();
        }
    }
#endif
}
