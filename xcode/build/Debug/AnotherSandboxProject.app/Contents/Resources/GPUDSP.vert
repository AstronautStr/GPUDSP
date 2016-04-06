#version 330 core

#define M_PI        3.14159265358979323846264338327950288
#define M_PI_2      1.57079632679489661923132169163975144
#define M_PI_4      0.78539816339744830961566084581987572
#define M_2PI       (2.0 * M_PI)

#define TEST_FREQUENCY 60.0

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

float testWaveTable()
{
    float time = float(samplesProcessed + samplePosition) / float(sampleRate);
    int tableIndex = int(fract(time * TEST_FREQUENCY) * float(sampleRate));
    
    return texelFetch(waveTable, tableIndex).x;
}

void main()
{
    //sampleValue = testSineWave();
    sampleValue = testWaveTable();
}