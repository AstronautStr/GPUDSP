#version 330 core

#define M_PI        3.14159265358979323846264338327950288
#define M_PI_2      1.57079632679489661923132169163975144
#define M_PI_4      0.78539816339744830961566084581987572
#define M_2PI       (2.0 * M_PI)

#define TEST_FREQUENCY 73.3

in int samplePosition;

uniform int samplesProcessed;
uniform int sampleRate;

uniform samplerBuffer waveTable;

out float sampleValue;

float testSineWave()
{
    float time = float(samplesProcessed + samplePosition) / float(sampleRate);
    return sin(M_2PI * time * TEST_FREQUENCY);
}

float testWaveTableInt()
{
    int fInt = int(TEST_FREQUENCY);
    float fFract = fract(TEST_FREQUENCY);
    
    int spmsr = samplesProcessed % sampleRate;
    int fimsr = fInt % sampleRate;
    int simsr = samplePosition % sampleRate;
    
    int aPart = (spmsr * fimsr) % sampleRate + (simsr * fimsr) % sampleRate;
    int bPart = int(float(samplesProcessed) * fFract) % sampleRate + int(float(samplePosition) * fFract) % sampleRate;
    
    int tableIndex = (aPart + bPart) % sampleRate;
    
    return texelFetch(waveTable, tableIndex).x;
}

float testWaveTable()
{
    float time = float(samplesProcessed + samplePosition) / float(sampleRate);
    int tableIndex = int(fract(time * TEST_FREQUENCY) * sampleRate + 0.5);
    
    return texelFetch(waveTable, tableIndex).x;
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

float waveTableOsc(float frequency)
{
    int     frecInt     = int(frequency);
    float   frecFract   = fract(frequency);
    int     SPmSR       = samplesProcessed % sampleRate;
    int     FImSR       = frecInt % sampleRate;
    int     SImSR       = samplePosition % sampleRate;
    int     intPart     = (SPmSR * FImSR) % sampleRate + (SImSR * FImSR) % sampleRate;
    int     fractPart   = int(float(samplesProcessed) * frecFract) % sampleRate + int(float(samplePosition) * frecFract) % sampleRate;
    
    int     tableIndex  = (intPart + fractPart) % sampleRate;
    
    return  texelFetch(waveTable, tableIndex).x;
}

void main()
{
    float value = 0.0;
    const float count = 64.0;
    for (int i = 0; i < int(count); ++i)
    {
        value += waveTableOsc(TEST_FREQUENCY + TEST_FREQUENCY * floor(10 * rand(vec2(0.1, float(i) / count))));
    }
    
    sampleValue = value / count;
}