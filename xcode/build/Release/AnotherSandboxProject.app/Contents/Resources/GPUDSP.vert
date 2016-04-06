#version 330 core

#define M_PI 3.14159265358979323846
#define M_2PI (2.0 * M_PI)

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
    int tableIndex = ((samplesProcessed + samplePosition) % sampleRate) * 60 % sampleRate;
    return texelFetch(cellsSampler, tableIndex);
}

void main()
{
    //sampleValue = testSineWave();
    sampleValue = testWaveTable();
}