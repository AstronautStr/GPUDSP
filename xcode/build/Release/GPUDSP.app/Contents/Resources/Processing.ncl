#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#define FLOATING_POINT_TYPE double

typedef SampleType      FLOATING_POINT_TYPE;
typedef SampleType2     FLOATING_POINT_TYPE2;
typedef SampleType4     FLOATING_POINT_TYPE4;

void osc(__global SampleType* samples, __global SampleType* waveTable, uint sampleRate, uint samplesProcessed, SampleType frequency)
{
    uint samplePosition = get_global_id(0);

    uint     frecInt     = uint(frequency);
    SampleType    frecFract;  fract(frequency, &frecFract);
    
    uint     SPmSR       = samplesProcessed % sampleRate;
    uint     FImSR       = frecInt % sampleRate;
    uint     SImSR       = samplePosition % sampleRate;
    uint     intPart     = (SPmSR * FImSR) % sampleRate + (SImSR * FImSR) % sampleRate;
    uint     fractPart   = uint(SampleType(samplesProcessed) * frecFract) % sampleRate + uint(SampleType(samplePosition) * frecFract) % sampleRate;
    
    uint     tableIndex  = (intPart + fractPart) % sampleRate;
    
    samples[samplePosition] = waveTable[tableIndex];
}

__kernel void kernelMain(__global SampleType* samples, __global SampleType* waveTable, uint sampleRate, uint samplesProcessed, uint bufferSize, __global SampleType4* cells, __global SampleType* rules, uint2 gridSize)
{
    uint globalID = get_global_id(0);
    uint size = gridSize.x * gridSize.y;
    
    samples[globalID] = 0.0f;
    SampleType power = 0.0f;
    for (int i = 0; i < size; ++i)
    {
        power += fabs(cells[i + globalID * size].x);
        samples[globalID] += cells[i + globalID * size].z;
    }
     
    samples[globalID] = samples[globalID] / power;
}