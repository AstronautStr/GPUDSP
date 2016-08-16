#ifdef cl_khr_fp64
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
typedef     double      DSPSampleType;
typedef     double2     DSPSampleType2;
typedef     double4     DSPSampleType4;
#else
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
typedef     float       DSPSampleType;
typedef     float2      DSPSampleType2;
typedef     float4      DSPSampleType4;
#endif

typedef     int         DSPSampleTypei;
#define     DSPSampleTypeiMax   INT_MAX

void osc(__global DSPSampleType* samples, __global DSPSampleType* waveTable, uint sampleRate, uint samplesProcessed, DSPSampleType frequency)
{
    uint samplePosition = get_global_id(0);

    uint     frecInt     = uint(frequency);
    DSPSampleType    frecFract;  fract(frequency, &frecFract);
    
    uint     SPmSR       = samplesProcessed % sampleRate;
    uint     FImSR       = frecInt % sampleRate;
    uint     SImSR       = samplePosition % sampleRate;
    uint     intPart     = (SPmSR * FImSR) % sampleRate + (SImSR * FImSR) % sampleRate;
    uint     fractPart   = uint(DSPSampleType(samplesProcessed) * frecFract) % sampleRate + uint(DSPSampleType(samplePosition) * frecFract) % sampleRate;
    
    uint     tableIndex  = (intPart + fractPart) % sampleRate;
    
    samples[samplePosition] = waveTable[tableIndex];
}

void processingFloat(__global DSPSampleType* samples, __global DSPSampleType* waveTable, uint sampleRate, uint samplesProcessed, uint bufferSize, __global DSPSampleType4* cells, uint2 gridSize)
{
    uint globalID = get_global_id(0);
    uint size = gridSize.x * gridSize.y;
    
    DSPSampleType power = 0.0f;
    samples[globalID] = 0.0f;
    
    DSPSampleType perr = 0.0f;
    DSPSampleType serr = 0.0f;
    for (int i = 0; i < size; ++i)
    {
        /*
        float pval = fabs(cells[i + globalID * size].x) - perr;
        float psum = power + pval;
        perr = (psum - power) - pval;
        power = psum;
        
        float sval = cells[i + globalID * size].z - serr;
        float ssum = samples[globalID] + sval;
        serr = (ssum - samples[globalID]) - sval;
        samples[globalID] = ssum;
         */
        DSPSampleType value = cells[i + globalID * size].x;
        DSPSampleType sval = value - serr;
        DSPSampleType ssum = samples[globalID] + sval;
        serr = (ssum - samples[globalID]) - sval;
        samples[globalID] = ssum;
    }
    
    samples[globalID] = samples[globalID] / DSPSampleType(size);
    //power += clamp(1.0f - power, 0.0f, 1.0f);
    //samples[globalID] = samples[globalID] / power;
}

__kernel void kernelMain(__global DSPSampleType* samples, __global DSPSampleType* waveTable, uint sampleRate, uint samplesProcessed, uint bufferSize, __global DSPSampleType4* cells, __global DSPSampleType* rules, uint2 gridSize)
{
    processingFloat(samples, waveTable, sampleRate, samplesProcessed, bufferSize, cells, gridSize);
}